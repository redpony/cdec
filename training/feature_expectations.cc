#include <sstream>
#include <iostream>
#include <fstream>
#include <vector>
#include <cassert>
#include <cmath>
#include <tr1/memory>

#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "verbose.h"
#include "hg.h"
#include "prob.h"
#include "inside_outside.h"
#include "ff_register.h"
#include "decoder.h"
#include "filelib.h"
#include "online_optimizer.h"
#include "fdict.h"
#include "weights.h"
#include "sparse_vector.h"
#include "sampler.h"

#ifdef HAVE_MPI
#include <boost/mpi/timer.hpp>
#include <boost/mpi.hpp>
namespace mpi = boost::mpi;
#endif

using namespace std;
namespace po = boost::program_options;

struct FComp {
  const vector<double>& w_;
  FComp(const vector<double>& w) : w_(w) {}
  bool operator()(int a, int b) const {
    return fabs(w_[a]) > fabs(w_[b]);
  }
};

void ShowFeatures(const vector<double>& w) {
  vector<int> fnums(w.size());
  for (int i = 0; i < w.size(); ++i)
    fnums[i] = i;
  sort(fnums.begin(), fnums.end(), FComp(w));
  for (vector<int>::iterator i = fnums.begin(); i != fnums.end(); ++i) {
    if (w[*i]) cout << FD::Convert(*i) << ' ' << w[*i] << endl;
  }
}

void ReadConfig(const string& ini, vector<string>* out) {
  ReadFile rf(ini);
  istream& in = *rf.stream();
  while(in) {
    string line;
    getline(in, line);
    if (!in) continue;
    out->push_back(line);
  }
}

void StoreConfig(const vector<string>& cfg, istringstream* o) {
  ostringstream os;
  for (int i = 0; i < cfg.size(); ++i) { os << cfg[i] << endl; }
  o->str(os.str());
}

bool InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("input,i",po::value<string>(),"Corpus of source language sentences")
        ("weights,w",po::value<string>(),"Input feature weights file")
        ("decoder_config,c",po::value<string>(), "cdec.ini file");
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

  if (conf->count("help") || !conf->count("input") || !conf->count("decoder_config")) {
    cerr << dcmdline_options << endl;
    return false;
  }
  return true;
}

void ReadTrainingCorpus(const string& fname, int rank, int size, vector<string>* c, vector<int>* order) {
  ReadFile rf(fname);
  istream& in = *rf.stream();
  string line;
  int id = 0;
  while(in) {
    getline(in, line);
    if (!in) break;
    if (id % size == rank) {
      c->push_back(line);
      order->push_back(id);
    }
    ++id;
  }
}

static const double kMINUS_EPSILON = -1e-6;

struct TrainingObserver : public DecoderObserver {
  void Reset() {
    acc_exp.clear();
    total_complete = 0;
  } 

  virtual void NotifyDecodingStart(const SentenceMetadata& smeta) {
    cur_model_exp.clear();
    state = 1;
  }

  // compute model expectations, denominator of objective
  virtual void NotifyTranslationForest(const SentenceMetadata& smeta, Hypergraph* hg) {
    assert(state == 1);
    state = 2;
    const prob_t z = InsideOutside<prob_t,
                                   EdgeProb,
                                   SparseVector<prob_t>,
                                   EdgeFeaturesAndProbWeightFunction>(*hg, &cur_model_exp);
    cur_model_exp /= z;
    acc_exp += cur_model_exp;
  }

  virtual void NotifyAlignmentForest(const SentenceMetadata& smeta, Hypergraph* hg) {
    cerr << "IGNORING ALIGNMENT FOREST!\n";
  }

  virtual void NotifyDecodingComplete(const SentenceMetadata& smeta) {
    if (state == 2) {
      ++total_complete;
    }
  }

  void GetExpectations(SparseVector<double>* g) const {
    g->clear();
    for (SparseVector<prob_t>::const_iterator it = acc_exp.begin(); it != acc_exp.end(); ++it)
      g->set_value(it->first, it->second);
  }

  int total_complete;
  SparseVector<prob_t> cur_model_exp;
  SparseVector<prob_t> acc_exp;
  int state;
};

#ifdef HAVE_MPI
namespace boost { namespace mpi {
  template<>
  struct is_commutative<std::plus<SparseVector<double> >, SparseVector<double> > 
    : mpl::true_ { };
} } // end namespace boost::mpi
#endif

int main(int argc, char** argv) {
#ifdef HAVE_MPI
  mpi::environment env(argc, argv);
  mpi::communicator world;
  const int size = world.size(); 
  const int rank = world.rank();
#else
  const int size = 1;
  const int rank = 0;
#endif
  if (size > 1) SetSilent(true);  // turn off verbose decoder output
  register_feature_functions();

  po::variables_map conf;
  if (!InitCommandLine(argc, argv, &conf))
    return 1;

  // load initial weights
  Weights weights;
  if (conf.count("weights"))
    weights.InitFromFile(conf["weights"].as<string>());

  vector<string> corpus;
  vector<int> ids;
  ReadTrainingCorpus(conf["input"].as<string>(), rank, size, &corpus, &ids);
  assert(corpus.size() > 0);

  vector<string> cdec_ini;
  ReadConfig(conf["decoder_config"].as<string>(), &cdec_ini);
  istringstream ini;
  StoreConfig(cdec_ini, &ini);
  Decoder decoder(&ini);
  if (decoder.GetConf()["input"].as<string>() != "-") {
    cerr << "cdec.ini must not set an input file\n";
    return 1;
  }

  SparseVector<double> x;
  weights.InitSparseVector(&x);
  TrainingObserver observer;

  weights.InitFromVector(x);
  vector<double> lambdas;
  weights.InitVector(&lambdas);
  decoder.SetWeights(lambdas);
  observer.Reset();
  for (unsigned i = 0; i < corpus.size(); ++i) {
    int id = ids[i];
    decoder.SetId(id);
    decoder.Decode(corpus[i], &observer);
  }
  SparseVector<double> local_exps, exps;
  observer.GetExpectations(&local_exps);
#ifdef HAVE_MPI
  reduce(world, local_exps, exps, std::plus<SparseVector<double> >(), 0);
#else
  exps.swap(local_exps);
#endif

  weights.InitFromVector(exps);
  weights.InitVector(&lambdas);
  ShowFeatures(lambdas);

  return 0;
}
