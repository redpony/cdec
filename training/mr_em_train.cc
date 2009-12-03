#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>

#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "config.h"
#ifdef HAVE_BOOST_DIGAMMA
#include <boost/math/special_functions/digamma.hpp>
using boost::math::digamma;
#endif

#include "tdict.h"
#include "filelib.h"
#include "trule.h"
#include "fdict.h"
#include "weights.h"
#include "sparse_vector.h"

using namespace std;
using boost::shared_ptr;
namespace po = boost::program_options;

#ifndef HAVE_BOOST_DIGAMMA
#warning Using Mark Johnson's digamma()
double digamma(double x) {
  double result = 0, xx, xx2, xx4;
  assert(x > 0);
  for ( ; x < 7; ++x)
    result -= 1/x;
  x -= 1.0/2.0;
  xx = 1.0/x;
  xx2 = xx*xx;
  xx4 = xx2*xx2;
  result += log(x)+(1./24.)*xx2-(7.0/960.0)*xx4+(31.0/8064.0)*xx4*xx2-(127.0/30720.0)*xx4*xx4;
  return result;
}
#endif

void SanityCheck(const vector<double>& w) {
  for (int i = 0; i < w.size(); ++i) {
    assert(!isnan(w[i]));
  }
}

struct FComp {
  const vector<double>& w_;
  FComp(const vector<double>& w) : w_(w) {}
  bool operator()(int a, int b) const {
    return w_[a] > w_[b];
  }
};

void ShowLargestFeatures(const vector<double>& w) {
  vector<int> fnums(w.size() - 1);
  for (int i = 1; i < w.size(); ++i)
    fnums[i-1] = i;
  vector<int>::iterator mid = fnums.begin();
  mid += (w.size() > 10 ? 10 : w.size()) - 1;
  partial_sort(fnums.begin(), mid, fnums.end(), FComp(w));
  cerr << "MOST PROBABLE:";
  for (vector<int>::iterator i = fnums.begin(); i != mid; ++i) {
    cerr << ' ' << FD::Convert(*i) << '=' << w[*i];
  }
  cerr << endl;
}

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("output,o",po::value<string>()->default_value("-"),"Output log probs file")
        ("grammar,g",po::value<vector<string> >()->composing(),"SCFG grammar file(s)")
        ("optimization_method,m", po::value<string>()->default_value("em"), "Optimization method (em, vb)")
        ("input_format,f",po::value<string>()->default_value("b64"),"Encoding of the input (b64 or text)");
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

  if (conf->count("help") || !conf->count("grammar")) {
    cerr << dcmdline_options << endl;
    exit(1);
  }
}

// describes a multinomial or multinomial with a prior
// does not contain the parameters- just the list of events
// and any hyperparameters
struct MultinomialInfo {
 MultinomialInfo() : alpha(1.0) {}
 vector<int> events;      // the events that this multinomial generates
 double alpha;            // hyperparameter for (optional) Dirichlet prior
};

typedef map<WordID, MultinomialInfo> ModelDefinition;

void LoadModelEvents(const po::variables_map& conf, ModelDefinition* pm) {
  ModelDefinition& m = *pm;
  m.clear();
  vector<string> gfiles = conf["grammar"].as<vector<string> >();
  for (int i = 0; i < gfiles.size(); ++i) {
    ReadFile rf(gfiles[i]);
    istream& in = *rf.stream();
    int lc = 0;
    while(in) {
      string line;
      getline(in, line);
      if (line.empty()) continue;
      ++lc;
      TRule r(line, true);
      const SparseVector<double>& f = r.GetFeatureValues();
      if (f.num_active() == 0) {
        cerr << "[WARNING] no feature found in " << gfiles[i] << ':' << lc << endl;
        continue;
      }
      if (f.num_active() > 1) {
        cerr << "[ERROR] more than one feature found in " << gfiles[i] << ':' << lc << endl;
        exit(1);
      }
      SparseVector<double>::const_iterator it = f.begin();
      if (it->second != 1.0) {
        cerr << "[ERROR] feature with value != 1 found in " << gfiles[i] << ':' << lc << endl;
        exit(1);
      }
      m[r.GetLHS()].events.push_back(it->first);
    }
  }
  for (ModelDefinition::iterator it = m.begin(); it != m.end(); ++it) {
    const vector<int>& v = it->second.events;
    cerr << "Multinomial [" << TD::Convert(it->first*-1) << "]\n";
    if (v.size() < 1000) {
      cerr << "  generates:";    
      for (int i = 0; i < v.size(); ++i) {
        cerr << " " << FD::Convert(v[i]);
      }
      cerr << endl;
    }
  }
}

void Maximize(const ModelDefinition& m, const bool use_vb, vector<double>* counts) {
  for (ModelDefinition::const_iterator it = m.begin(); it != m.end(); ++it) {
    const MultinomialInfo& mult_info = it->second;
    const vector<int>& events = mult_info.events;
    cerr << "Multinomial [" << TD::Convert(it->first*-1) << "]";
    double tot = 0;
    for (int i = 0; i < events.size(); ++i)
      tot += (*counts)[events[i]];
    cerr << " = " << tot << endl;
    assert(tot > 0.0);
    double ltot = log(tot);
    if (use_vb)
      ltot = digamma(tot + events.size() * mult_info.alpha);
    for (int i = 0; i < events.size(); ++i) {
      if (use_vb) {
        (*counts)[events[i]] = digamma((*counts)[events[i]] + mult_info.alpha) - ltot;
      } else {
        (*counts)[events[i]] = log((*counts)[events[i]]) - ltot;
      }
    }
    if (events.size() < 50) {
      for (int i = 0; i < events.size(); ++i) {
        cerr << " p(" << FD::Convert(events[i]) << ")=" << exp((*counts)[events[i]]);
      }
      cerr << endl;
    }
  }
}

int main(int argc, char** argv) {
  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);

  const bool use_b64 = conf["input_format"].as<string>() == "b64";
  const bool use_vb = conf["optimization_method"].as<string>() == "vb";
  if (use_vb)
    cerr << "Using variational Bayes, make sure alphas are set\n";

  ModelDefinition model_def;
  LoadModelEvents(conf, &model_def);

  const string s_obj = "**OBJ**";
  int num_feats = FD::NumFeats();
  cerr << "Number of features: " << num_feats << endl;

  vector<double> counts(num_feats, 0);
  double logprob = 0;
  // 0<TAB>**OBJ**=12.2;Feat1=2.3;Feat2=-0.2;
  // 0<TAB>**OBJ**=1.1;Feat1=1.0;

  // E-step
  while(cin) {
    string line;
    getline(cin, line);
    if (line.empty()) continue;
    int feat;
    double val;
    size_t i = line.find("\t");
    assert(i != string::npos);
    ++i;
    if (use_b64) {
      SparseVector<double> g;
      double obj;
      if (!B64::Decode(&obj, &g, &line[i], line.size() - i)) {
        cerr << "B64 decoder returned error, skipping!\n";
        continue;
      }
      logprob += obj;
      const SparseVector<double>& cg = g;
      for (SparseVector<double>::const_iterator it = cg.begin(); it != cg.end(); ++it) {
        if (it->first >= num_feats) {
	  cerr << "Unexpected feature: " << FD::Convert(it->first) << endl;
	  abort();
        }
        counts[it->first] += it->second;
      }
    } else {       // text encoding - your counts will not be accurate!
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
	    cerr << "Unexpected feature: " << line.substr(start, i - start) << endl;
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
          logprob += val;
        } else {
          counts[feat] += val;
        }
      }
    }
  }

  cerr << "LOGPROB: " << logprob << endl;
  // M-step
  Maximize(model_def, use_vb, &counts);

  SanityCheck(counts);
  ShowLargestFeatures(counts);
  Weights weights;
  weights.InitFromVector(counts);
  weights.WriteToFile(conf["output"].as<string>(), false);

  return 0;
}
