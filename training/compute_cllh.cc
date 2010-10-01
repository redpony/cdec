#include <sstream>
#include <iostream>
#include <fstream>
#include <vector>
#include <cassert>
#include <cmath>

#include <mpi.h>
#include <boost/mpi.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "verbose.h"
#include "hg.h"
#include "prob.h"
#include "inside_outside.h"
#include "ff_register.h"
#include "decoder.h"
#include "filelib.h"
#include "weights.h"

using namespace std;
namespace po = boost::program_options;

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("weights,w",po::value<string>(),"Input feature weights file")
        ("training_data,t",po::value<string>(),"Training data corpus")
        ("decoder_config,c",po::value<string>(),"Decoder configuration file");
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

  if (conf->count("help") || !conf->count("training_data") || !conf->count("decoder_config")) {
    cerr << dcmdline_options << endl;
    MPI::Finalize();
    exit(1);
  }
}

void ReadTrainingCorpus(const string& fname, int rank, int size, vector<string>* c, vector<int>* ids) {
  ReadFile rf(fname);
  istream& in = *rf.stream();
  string line;
  int lc = 0;
  while(in) {
    getline(in, line);
    if (!in) break;
    if (lc % size == rank) {
      c->push_back(line);
      ids->push_back(lc);
    }
    ++lc;
  }
}

static const double kMINUS_EPSILON = -1e-6;

struct TrainingObserver : public DecoderObserver {
  void Reset() {
    acc_obj = 0;
  } 

  virtual void NotifyDecodingStart(const SentenceMetadata&) {
    cur_obj = 0;
    state = 1;
  }

  // compute model expectations, denominator of objective
  virtual void NotifyTranslationForest(const SentenceMetadata&, Hypergraph* hg) {
    assert(state == 1);
    state = 2;
    SparseVector<prob_t> cur_model_exp;
    const prob_t z = InsideOutside<prob_t,
                                   EdgeProb,
                                   SparseVector<prob_t>,
                                   EdgeFeaturesAndProbWeightFunction>(*hg, &cur_model_exp);
    cur_obj = log(z);
  }

  // compute "empirical" expectations, numerator of objective
  virtual void NotifyAlignmentForest(const SentenceMetadata& smeta, Hypergraph* hg) {
    assert(state == 2);
    state = 3;
    SparseVector<prob_t> ref_exp;
    const prob_t ref_z = InsideOutside<prob_t,
                                       EdgeProb,
                                       SparseVector<prob_t>,
                                       EdgeFeaturesAndProbWeightFunction>(*hg, &ref_exp);

    double log_ref_z;
#if 0
    if (crf_uniform_empirical) {
      log_ref_z = ref_exp.dot(feature_weights);
    } else {
      log_ref_z = log(ref_z);
    }
#else
    log_ref_z = log(ref_z);
#endif

    // rounding errors means that <0 is too strict
    if ((cur_obj - log_ref_z) < kMINUS_EPSILON) {
      cerr << "DIFF. ERR! log_model_z < log_ref_z: " << cur_obj << " " << log_ref_z << endl;
      exit(1);
    }
    assert(!isnan(log_ref_z));
    acc_obj += (cur_obj - log_ref_z);
  }

  double acc_obj;
  double cur_obj;
  int state;
};

namespace mpi = boost::mpi;

int main(int argc, char** argv) {
  mpi::environment env(argc, argv);
  mpi::communicator world;
  const int size = world.size(); 
  const int rank = world.rank();
  if (size > 1) SetSilent(true);  // turn off verbose decoder output
  register_feature_functions();

  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);

  // load initial weights
  Weights weights;
  if (conf.count("weights"))
    weights.InitFromFile(conf["weights"].as<string>());

  // freeze feature set
  //const bool freeze_feature_set = conf.count("freeze_feature_set");
  //if (freeze_feature_set) FD::Freeze();

  // load cdec.ini and set up decoder
  ReadFile ini_rf(conf["decoder_config"].as<string>());
  Decoder decoder(ini_rf.stream());
  if (decoder.GetConf()["input"].as<string>() != "-") {
    cerr << "cdec.ini must not set an input file\n";
    abort();
  }

  vector<string> corpus; vector<int> ids;
  ReadTrainingCorpus(conf["training_data"].as<string>(), rank, size, &corpus, &ids);
  assert(corpus.size() > 0);
  assert(corpus.size() == ids.size());

  vector<double> wv;
  weights.InitVector(&wv);
  decoder.SetWeights(wv);
  TrainingObserver observer;
  double objective = 0;
  bool converged = false;

  observer.Reset();
  if (rank == 0)
    cerr << "Each processor is decoding " << corpus.size() << " training examples...\n";

  for (int i = 0; i < corpus.size(); ++i) {
    decoder.SetId(ids[i]);
    decoder.Decode(corpus[i], &observer);
  }

  reduce(world, observer.acc_obj, objective, std::plus<double>(), 0);

  if (rank == 0)
    cout << "OBJECTIVE: " << objective << endl;

  return 0;
}
