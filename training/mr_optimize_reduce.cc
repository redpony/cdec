#include <sstream>
#include <iostream>
#include <fstream>
#include <vector>
#include <cassert>
#include <cmath>

#include <boost/shared_ptr.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "optimize.h"
#include "fdict.h"
#include "weights.h"
#include "sparse_vector.h"

using namespace std;
namespace po = boost::program_options;

void SanityCheck(const vector<double>& w) {
  for (int i = 0; i < w.size(); ++i) {
    assert(!std::isnan(w[i]));
    assert(!std::isinf(w[i]));
  }
}

struct FComp {
  const vector<double>& w_;
  FComp(const vector<double>& w) : w_(w) {}
  bool operator()(int a, int b) const {
    return fabs(w_[a]) > fabs(w_[b]);
  }
};

void ShowLargestFeatures(const vector<double>& w) {
  vector<int> fnums(w.size());
  for (int i = 0; i < w.size(); ++i)
    fnums[i] = i;
  vector<int>::iterator mid = fnums.begin();
  mid += (w.size() > 10 ? 10 : w.size());
  partial_sort(fnums.begin(), mid, fnums.end(), FComp(w));
  cerr << "TOP FEATURES:";
  for (vector<int>::iterator i = fnums.begin(); i != mid; ++i) {
    cerr << ' ' << FD::Convert(*i) << '=' << w[*i];
  }
  cerr << endl;
}

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("input_weights,i",po::value<string>(),"Input feature weights file")
        ("output_weights,o",po::value<string>()->default_value("-"),"Output feature weights file")
        ("optimization_method,m", po::value<string>()->default_value("lbfgs"), "Optimization method (sgd, lbfgs, rprop)")
        ("state,s",po::value<string>(),"Read (and write if output_state is not set) optimizer state from this state file. In the first iteration, the file should not exist.")
        ("input_format,f",po::value<string>()->default_value("b64"),"Encoding of the input (b64 or text)")
        ("output_state,S", po::value<string>(), "Output state file (optional override)")
	("correction_buffers,M", po::value<int>()->default_value(10), "Number of gradients for LBFGS to maintain in memory")
        ("eta,e", po::value<double>()->default_value(0.1), "Learning rate for SGD (eta)")
        ("gaussian_prior,p","Use a Gaussian prior on the weights")
        ("means,u", po::value<string>(), "File containing the means for Gaussian prior")
        ("sigma_squared", po::value<double>()->default_value(1.0), "Sigma squared term for spherical Gaussian prior");
  po::options_description clo("Command line options");
  clo.add_options()
        ("config", po::value<string>(), "Configuration file")
        ("help,h", "Print this help message and exit");
  po::options_description dconfig_options, dcmdline_options;
  dconfig_options.add(opts);
  dcmdline_options.add(opts).add(clo);
  
  po::store(parse_command_line(argc, argv, dcmdline_options), *conf);
  if (conf->count("config")) {
    ifstream config((*conf)["config"].as<string>().c_str());
    po::store(po::parse_config_file(config, dconfig_options), *conf);
  }
  po::notify(*conf);

  if (conf->count("help") || !conf->count("input_weights") || !conf->count("state")) {
    cerr << dcmdline_options << endl;
    exit(1);
  }
}

int main(int argc, char** argv) {
  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);

  const bool use_b64 = conf["input_format"].as<string>() == "b64";

  vector<weight_t> lambdas;
  Weights::InitFromFile(conf["input_weights"].as<string>(), &lambdas);
  const string s_obj = "**OBJ**";
  int num_feats = FD::NumFeats();
  cerr << "Number of features: " << num_feats << endl;
  const bool gaussian_prior = conf.count("gaussian_prior");
  vector<weight_t> means(num_feats, 0);
  if (conf.count("means")) {
    if (!gaussian_prior) {
      cerr << "Don't use --means without --gaussian_prior!\n";
      exit(1);
    }
    Weights::InitFromFile(conf["means"].as<string>(), &means);
  }
  boost::shared_ptr<BatchOptimizer> o;
  const string omethod = conf["optimization_method"].as<string>();
  if (omethod == "rprop")
    o.reset(new RPropOptimizer(num_feats));  // TODO add configuration
  else
    o.reset(new LBFGSOptimizer(num_feats, conf["correction_buffers"].as<int>()));
  cerr << "Optimizer: " << o->Name() << endl;
  string state_file = conf["state"].as<string>();
  {
    ifstream in(state_file.c_str(), ios::binary);
    if (in)
      o->Load(&in);
    else
      cerr << "No state file found, assuming ITERATION 1\n";
  }

  double objective = 0;
  vector<double> gradient(num_feats, 0);
  // 0<TAB>**OBJ**=12.2;Feat1=2.3;Feat2=-0.2;
  // 0<TAB>**OBJ**=1.1;Feat1=1.0;
  int total_lines = 0;  // TODO - this should be a count of the
                        // training instances!!
  while(cin) {
    string line;
    getline(cin, line);
    if (line.empty()) continue;
    ++total_lines;
    int feat;
    double val;
    size_t i = line.find("\t");
    assert(i != string::npos);
    ++i;
    if (use_b64) {
      SparseVector<double> g;
      double obj;
      if (!B64::Decode(&obj, &g, &line[i], line.size() - i)) {
        cerr << "B64 decoder returned error, skipping gradient!\n";
	cerr << "  START: " << line.substr(0,line.size() > 200 ? 200 : line.size()) << endl;
	if (line.size() > 200)
	  cerr << "    END: " << line.substr(line.size() - 200, 200) << endl;
        cout << "-1\tRESTART\n";
        exit(99);
      }
      objective += obj;
      const SparseVector<double>& cg = g;
      for (SparseVector<double>::const_iterator it = cg.begin(); it != cg.end(); ++it) {
        if (it->first >= num_feats) {
	  cerr << "Unexpected feature in gradient: " << FD::Convert(it->first) << endl;
	  abort();
        }
        gradient[it->first] -= it->second;
      }
    } else {       // text encoding - your gradients will not be accurate!
      while (i < line.size()) {
        size_t start = i;
        while (line[i] != '=' && i < line.size()) ++i;
        if (i == line.size()) { cerr << "FORMAT ERROR\n"; break; }
        string fname = line.substr(start, i - start);
        if (fname == s_obj) {
          feat = -1;
        } else {
          feat = FD::Convert(line.substr(start, i - start));
          if (feat >= num_feats) {
	    cerr << "Unexpected feature in gradient: " << line.substr(start, i - start) << endl;
	    abort();
	  }
        }
        ++i;
        start = i;
        while (line[i] != ';' && i < line.size()) ++i;
        if (i - start == 0) continue;
        val = atof(line.substr(start, i - start).c_str());
        ++i;
        if (feat == -1) {
          objective += val;
        } else {
          gradient[feat] -= val;
        }
      }
    }
  }

  if (gaussian_prior) {
    const double sigsq = conf["sigma_squared"].as<double>();
    double norm = 0;
    for (int k = 1; k < lambdas.size(); ++k) {
      const double& lambda_k = lambdas[k];
      if (lambda_k) {
        const double param = (lambda_k - means[k]);
        norm += param * param;
        gradient[k] += param / sigsq;
      }
    }
    const double reg = norm / (2.0 * sigsq);
    cerr << "REGULARIZATION TERM: " << reg << endl;
    objective += reg;
  }
  cerr << "EVALUATION #" << o->EvaluationCount() << " OBJECTIVE: " << objective << endl;
  double gnorm = 0;
  for (int i = 0; i < gradient.size(); ++i)
    gnorm += gradient[i] * gradient[i];
  cerr << "  GNORM=" << sqrt(gnorm) << endl;
  vector<double> old = lambdas;
  int c = 0;
  while (old == lambdas) {
    ++c;
    if (c > 1) { cerr << "Same lambdas, repeating optimization\n"; }
    o->Optimize(objective, gradient, &lambdas);
    assert(c < 5);
  }
  old.clear();
  SanityCheck(lambdas);
  ShowLargestFeatures(lambdas);
  Weights::WriteToFile(conf["output_weights"].as<string>(), lambdas, false);

  const bool conv = o->HasConverged();
  if (conv) { cerr << "OPTIMIZER REPORTS CONVERGENCE!\n"; }
  
  if (conf.count("output_state"))
    state_file = conf["output_state"].as<string>();
  ofstream out(state_file.c_str(), ios::binary);
  cerr << "Writing state to: " << state_file << endl;
  o->Save(&out);
  out.close();

  cout << o->EvaluationCount() << "\t" << conv << endl;
  return 0;
}
