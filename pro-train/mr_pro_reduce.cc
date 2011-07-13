#include <cstdlib>
#include <sstream>
#include <iostream>
#include <fstream>
#include <vector>

#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "weights.h"
#include "sparse_vector.h"
#include "optimize.h"

using namespace std;
namespace po = boost::program_options;

// since this is a ranking model, there should be equal numbers of
// positive and negative examples so the bias should be 0
static const double MAX_BIAS = 1e-10;

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("weights,w", po::value<string>(), "Weights from previous iteration (used as initialization and interpolation")
        ("interpolation,p",po::value<double>()->default_value(0.9), "Output weights are p*w + (1-p)*w_prev")
        ("memory_buffers,m",po::value<unsigned>()->default_value(200), "Number of memory buffers (LBFGS)")
        ("sigma_squared,s",po::value<double>()->default_value(1.0), "Sigma squared for Gaussian prior")
        ("help,h", "Help");
  po::options_description dcmdline_options;
  dcmdline_options.add(opts);
  po::store(parse_command_line(argc, argv, dcmdline_options), *conf);
  if (conf->count("help")) {
    cerr << dcmdline_options << endl;
    exit(1);
  }
}

void ParseSparseVector(string& line, size_t cur, SparseVector<double>* out) {
  SparseVector<double>& x = *out;
  size_t last_start = cur;
  size_t last_comma = string::npos;
  while(cur <= line.size()) {
    if (line[cur] == ' ' || cur == line.size()) {
      if (!(cur > last_start && last_comma != string::npos && cur > last_comma)) {
        cerr << "[ERROR] " << line << endl << "  position = " << cur << endl;
        exit(1);
      }
      const int fid = FD::Convert(line.substr(last_start, last_comma - last_start));
      if (cur < line.size()) line[cur] = 0;
      const double val = strtod(&line[last_comma + 1], NULL);
      x.set_value(fid, val);

      last_comma = string::npos;
      last_start = cur+1;
    } else {
      if (line[cur] == '=')
        last_comma = cur;
    }
    ++cur;
  }
}

int main(int argc, char** argv) {
  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);
  string line;
  vector<pair<bool, SparseVector<double> > > training;
  int lc = 0;
  bool flag = false;
  SparseVector<double> old_weights;
  const double psi = conf["interpolation"].as<double>();
  if (psi < 0.0 || psi > 1.0) { cerr << "Invalid interpolation weight: " << psi << endl; }
  if (conf.count("weights")) {
    Weights w;
    w.InitFromFile(conf["weights"].as<string>());
    w.InitSparseVector(&old_weights);
  }
  while(getline(cin, line)) {
    ++lc;
    if (lc % 1000 == 0) { cerr << '.'; flag = true; }
    if (lc % 40000 == 0) { cerr << " [" << lc << "]\n"; flag = false; }
    if (line.empty()) continue;
    const size_t ks = line.find("\t");
    assert(string::npos != ks);
    assert(ks == 1);
    const bool y = line[0] == '1';
    SparseVector<double> x;
    ParseSparseVector(line, ks + 1, &x);
    training.push_back(make_pair(y, x));
  }
  if (flag) cerr << endl;

  cerr << "Number of features: " << FD::NumFeats() << endl;
  vector<double> x(FD::NumFeats(), 0.0);  // x[0] is bias
  for (SparseVector<double>::const_iterator it = old_weights.begin();
       it != old_weights.end(); ++it)
    x[it->first] = it->second;
  vector<double> vg(FD::NumFeats(), 0.0);
  SparseVector<double> g;
  bool converged = false;
  LBFGSOptimizer opt(FD::NumFeats(), conf["memory_buffers"].as<unsigned>());
  double ppl = 0;
  while(!converged) {
    double cll = 0;
    double dbias = 0;
    g.clear();
    for (int i = 0; i < training.size(); ++i) {
      const double dotprod = training[i].second.dot(x) + x[0]; // x[0] is bias
      double lp_false = dotprod;
      double lp_true = -dotprod;
      if (0 < lp_true) {
        lp_true += log1p(exp(-lp_true));
        lp_false = log1p(exp(lp_false));
      } else {
        lp_true = log1p(exp(lp_true));
        lp_false += log1p(exp(-lp_false));
      }
      lp_true*=-1;
      lp_false*=-1;
      if (training[i].first) {  // true label
        cll -= lp_true;
        ppl += lp_true / log(2);
        g -= training[i].second * exp(lp_false);
        dbias -= exp(lp_false);
      } else {                  // false label
        cll -= lp_false;
        ppl += lp_false / log(2);
        g += training[i].second * exp(lp_true);
        dbias += exp(lp_true);
      }
    }
    ppl /= training.size();
    ppl = pow(2.0, - ppl);
    vg.clear();
    g.init_vector(&vg);
    vg[0] = dbias;
#if 1
    const double sigsq = conf["sigma_squared"].as<double>();
    double norm = 0;
    for (int i = 1; i < x.size(); ++i) {
      const double mean_i = 0.0;
      const double param = (x[i] - mean_i);
      norm += param * param;
      vg[i] += param / sigsq;
    } 
    const double reg = norm / (2.0 * sigsq);
#else
    double reg = 0;
#endif
    cll += reg;
    cerr << cll << " (REG=" << reg << ")\tPPL=" << ppl << "\t";
    bool failed = false;
    try {
      opt.Optimize(cll, vg, &x);
    } catch (...) {
      cerr << "Exception caught, assuming convergence is close enough...\n";
      failed = true;
    }
    if (fabs(x[0]) > MAX_BIAS) {
      cerr << "Biased model learned. Are your training instances wrong?\n";
      cerr << "  BIAS: " << x[0] << endl;
    }
    converged = failed || opt.HasConverged();
  }
  Weights w;
  if (conf.count("weights")) {
    for (int i = 1; i < x.size(); ++i)
      x[i] = (x[i] * psi) + old_weights.get(i) * (1.0 - psi);
  }
  w.InitFromVector(x);
  w.WriteToFile("-");
  return 0;
}
