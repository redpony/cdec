#include <cstdlib>
#include <sstream>
#include <iostream>
#include <fstream>
#include <vector>

#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "filelib.h"
#include "weights.h"
#include "sparse_vector.h"
#include "optimize.h"
#include "liblbfgs/lbfgs++.h"

using namespace std;
namespace po = boost::program_options;

// since this is a ranking model, there should be equal numbers of
// positive and negative examples, so the bias should be 0
static const double MAX_BIAS = 1e-10;

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("weights,w", po::value<string>(), "Weights from previous iteration (used as initialization and interpolation")
        ("regularization_strength,C",po::value<double>()->default_value(500.0), "l2 regularization strength")
        ("l1",po::value<double>()->default_value(0.0), "l1 regularization strength")
        ("regularize_to_weights,y",po::value<double>()->default_value(5000.0), "Differences in learned weights to previous weights are penalized with an l2 penalty with this strength; 0.0 = no effect")
        ("memory_buffers,m",po::value<unsigned>()->default_value(100), "Number of memory buffers (LBFGS)")
        ("min_reg,r",po::value<double>()->default_value(0.01), "When tuning (-T) regularization strength, minimum regularization strenght")
        ("max_reg,R",po::value<double>()->default_value(1e6), "When tuning (-T) regularization strength, maximum regularization strenght")
        ("testset,t",po::value<string>(), "Optional held-out test set")
        ("tune_regularizer,T", "Use the held out test set (-t) to tune the regularization strength")
        ("interpolate_with_weights,p",po::value<double>()->default_value(1.0), "[deprecated] Output weights are p*w + (1-p)*w_prev; 1.0 = no effect")
        ("help,h", "Help");
  po::options_description dcmdline_options;
  dcmdline_options.add(opts);
  po::store(parse_command_line(argc, argv, dcmdline_options), *conf);
  if (conf->count("help")) {
    cerr << dcmdline_options << endl;
    exit(1);
  }
}

void ParseSparseVector(string& line, size_t cur, SparseVector<weight_t>* out) {
  SparseVector<weight_t>& x = *out;
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
      const weight_t val = strtod(&line[last_comma + 1], NULL);
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

void ReadCorpus(istream* pin, vector<pair<bool, SparseVector<weight_t> > >* corpus) {
  istream& in = *pin;
  corpus->clear();
  bool flag = false;
  int lc = 0;
  string line;
  SparseVector<weight_t> x;
  while(getline(in, line)) {
    ++lc;
    if (lc % 1000 == 0) { cerr << '.'; flag = true; }
    if (lc % 40000 == 0) { cerr << " [" << lc << "]\n"; flag = false; }
    if (line.empty()) continue;
    const size_t ks = line.find("\t");
    assert(string::npos != ks);
    assert(ks == 1);
    const bool y = line[0] == '1';
    x.clear();
    ParseSparseVector(line, ks + 1, &x);
    corpus->push_back(make_pair(y, x));
  }
  if (flag) cerr << endl;
}

void GradAdd(const SparseVector<weight_t>& v, const double scale, weight_t* acc) {
  for (SparseVector<weight_t>::const_iterator it = v.begin();
       it != v.end(); ++it) {
    acc[it->first] += it->second * scale;
  }
}

double ApplyRegularizationTerms(const double C,
                                const double T,
                                const vector<weight_t>& weights,
                                const vector<weight_t>& prev_weights,
                                weight_t* g) {
  double reg = 0;
  for (size_t i = 0; i < weights.size(); ++i) {
    const double prev_w_i = (i < prev_weights.size() ? prev_weights[i] : 0.0);
    const double& w_i = weights[i];
    reg += C * w_i * w_i;
    g[i] += 2 * C * w_i;

    const double diff_i = w_i - prev_w_i;
    reg += T * diff_i * diff_i;
    g[i] += 2 * T * diff_i;
  }
  return reg;
}

double TrainingInference(const vector<weight_t>& x,
                         const vector<pair<bool, SparseVector<weight_t> > >& corpus,
                         weight_t* g = NULL) {
  double cll = 0;
  for (int i = 0; i < corpus.size(); ++i) {
    const double dotprod = corpus[i].second.dot(x) + (x.size() ? x[0] : weight_t()); // x[0] is bias
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
    if (corpus[i].first) {  // true label
      cll -= lp_true;
      if (g) {
        // g -= corpus[i].second * exp(lp_false);
        GradAdd(corpus[i].second, -exp(lp_false), g);
        g[0] -= exp(lp_false); // bias
      }
    } else {                  // false label
      cll -= lp_false;
      if (g) {
        // g += corpus[i].second * exp(lp_true);
        GradAdd(corpus[i].second, exp(lp_true), g);
        g[0] += exp(lp_true); // bias
      }
    }
  }
  return cll;
}

struct ProLoss {
  ProLoss(const vector<pair<bool, SparseVector<weight_t> > >& tr,
          const vector<pair<bool, SparseVector<weight_t> > >& te,
          const double c,
          const double t,
          const vector<weight_t>& px) : training(tr), testing(te), C(c), T(t), prev_x(px){}
  double operator()(const vector<double>& x, double* g) const {
    fill(g, g + x.size(), 0.0);
    double cll = TrainingInference(x, training, g);
    tppl = 0;
    if (testing.size())
      tppl = pow(2.0, TrainingInference(x, testing, g) / (log(2) * testing.size()));
    double ppl = cll / log(2);
    ppl /= training.size();
    ppl = pow(2.0, ppl);
    double reg = ApplyRegularizationTerms(C, T, x, prev_x, g);
    return cll + reg;
  }
  const vector<pair<bool, SparseVector<weight_t> > >& training, testing;
  const double C, T;
  const vector<double>& prev_x;
  mutable double tppl;
};

// return held-out log likelihood
double LearnParameters(const vector<pair<bool, SparseVector<weight_t> > >& training,
                       const vector<pair<bool, SparseVector<weight_t> > >& testing,
                       const double C,
                       const double C1,
                       const double T,
                       const unsigned memory_buffers,
                       const vector<weight_t>& prev_x,
                       vector<weight_t>* px) {
  assert(px->size() == prev_x.size());
  ProLoss loss(training, testing, C, T, prev_x);
  LBFGS<ProLoss> lbfgs(px, loss, memory_buffers, C1);
  lbfgs.MinimizeFunction();
  return loss.tppl;
}

int main(int argc, char** argv) {
  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);
  string line;
  vector<pair<bool, SparseVector<weight_t> > > training, testing;
  const bool tune_regularizer = conf.count("tune_regularizer");
  if (tune_regularizer && !conf.count("testset")) {
    cerr << "--tune_regularizer requires --testset to be set\n";
    return 1;
  }
  const double min_reg = conf["min_reg"].as<double>();
  const double max_reg = conf["max_reg"].as<double>();
  double C = conf["regularization_strength"].as<double>(); // will be overridden if parameter is tuned
  double C1 = conf["l1"].as<double>(); // will be overridden if parameter is tuned
  const double T = conf["regularize_to_weights"].as<double>();
  assert(C >= 0.0);
  assert(min_reg >= 0.0);
  assert(max_reg >= 0.0);
  assert(max_reg > min_reg);
  const double psi = conf["interpolate_with_weights"].as<double>();
  if (psi < 0.0 || psi > 1.0) { cerr << "Invalid interpolation weight: " << psi << endl; return 1; }
  ReadCorpus(&cin, &training);
  if (conf.count("testset")) {
    ReadFile rf(conf["testset"].as<string>());
    ReadCorpus(rf.stream(), &testing);
  }
  cerr << "Number of features: " << FD::NumFeats() << endl;

  vector<weight_t> x, prev_x;  // x[0] is bias
  if (conf.count("weights")) {
    Weights::InitFromFile(conf["weights"].as<string>(), &x);
    x.resize(FD::NumFeats());
    prev_x = x;
  } else {
    x.resize(FD::NumFeats());
    prev_x = x;
  }
  cerr << "         Number of features: " << x.size() << endl;
  cerr << "Number of training examples: " << training.size() << endl;
  cerr << "Number of  testing examples: " << testing.size() << endl;
  double tppl = 0.0;
  vector<pair<double,double> > sp;
  vector<double> smoothed;
  if (tune_regularizer) {
    C = min_reg;
    const double steps = 18;
    double sweep_factor = exp((log(max_reg) - log(min_reg)) / steps);
    cerr << "SWEEP FACTOR: " << sweep_factor << endl;
    while(C < max_reg) {
      cerr << "C=" << C << "\tT=" <<T << endl;
      tppl = LearnParameters(training, testing, C, C1, T, conf["memory_buffers"].as<unsigned>(), prev_x, &x);
      sp.push_back(make_pair(C, tppl));
      C *= sweep_factor;
    }
    smoothed.resize(sp.size(), 0);
    smoothed[0] = sp[0].second;
    smoothed.back() = sp.back().second; 
    for (int i = 1; i < sp.size()-1; ++i) {
      double prev = sp[i-1].second;
      double next = sp[i+1].second;
      double cur = sp[i].second;
      smoothed[i] = (prev*0.2) + cur * 0.6 + (0.2*next);
    }
    double best_ppl = 9999999;
    unsigned best_i = 0;
    for (unsigned i = 0; i < sp.size(); ++i) {
      if (smoothed[i] < best_ppl) {
        best_ppl = smoothed[i];
        best_i = i;
      }
    }
    C = sp[best_i].first;
  }  // tune regularizer
  tppl = LearnParameters(training, testing, C, C1, T, conf["memory_buffers"].as<unsigned>(), prev_x, &x);
  if (conf.count("weights")) {
    for (int i = 1; i < x.size(); ++i) {
      x[i] = (x[i] * psi) + prev_x[i] * (1.0 - psi);
    }
  }
  cout.precision(15);
  cout << "# C=" << C << "\theld out perplexity=";
  if (tppl) { cout << tppl << endl; } else { cout << "N/A\n"; }
  if (sp.size()) {
    cout << "# Parameter sweep:\n";
    for (int i = 0; i < sp.size(); ++i) {
      cout << "# " << sp[i].first << "\t" << sp[i].second << "\t" << smoothed[i] << endl;
    }
  }
  Weights::WriteToFile("-", x);
  return 0;
}
