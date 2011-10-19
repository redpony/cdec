#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>

#include "config.h"
#ifdef HAVE_MPI
#include <boost/mpi.hpp>
#endif
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "sentence_metadata.h"
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

bool InitCommandLine(int argc, char** argv, po::variables_map* conf) {
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
    return false;
  }
  return true;
}

void ReadInstances(const string& fname, int rank, int size, vector<string>* c) {
  assert(fname != "-");
  ReadFile rf(fname);
  istream& in = *rf.stream();
  string line;
  int lc = 0;
  while(in) {
    getline(in, line);
    if (!in) break;
    if (lc % size == rank) c->push_back(line);
    ++lc;
  }
}

static const double kMINUS_EPSILON = -1e-6;

struct ConditionalLikelihoodObserver : public DecoderObserver {

  ConditionalLikelihoodObserver() : trg_words(), acc_obj(), cur_obj() {}

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
    trg_words += smeta.GetReference().size();
  }

  unsigned trg_words;
  double acc_obj;
  double cur_obj;
  int state;
};

#ifdef HAVE_MPI
namespace mpi = boost::mpi;
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
    return false;

  // load cdec.ini and set up decoder
  ReadFile ini_rf(conf["decoder_config"].as<string>());
  Decoder decoder(ini_rf.stream());
  if (decoder.GetConf()["input"].as<string>() != "-") {
    cerr << "cdec.ini must not set an input file\n";
    abort();
  }

  // load weights
  vector<weight_t>& weights = decoder.CurrentWeightVector();
  if (conf.count("weights"))
    Weights::InitFromFile(conf["weights"].as<string>(), &weights);

  vector<string> corpus;
  ReadInstances(conf["training_data"].as<string>(), rank, size, &corpus);
  assert(corpus.size() > 0);

  if (rank == 0)
    cerr << "Each processor is decoding ~" << corpus.size() << " training examples...\n";

  ConditionalLikelihoodObserver observer;
  for (int i = 0; i < corpus.size(); ++i)
    decoder.Decode(corpus[i], &observer);

  double objective = 0;
  unsigned total_words = 0;
#ifdef HAVE_MPI
  reduce(world, observer.acc_obj, objective, std::plus<double>(), 0);
  reduce(world, observer.trg_words, total_words, std::plus<unsigned>(), 0);
#else
  objective = observer.acc_obj;
#endif

  if (rank == 0) {
    cout << "CONDITIONAL LOG_e LIKELIHOOD: " << objective << endl;
    cout << "CONDITIONAL LOG_2 LIKELIHOOD: " << (objective/log(2)) << endl;
    cout << "         CONDITIONAL ENTROPY: " << (objective/log(2) / total_words) << endl;
    cout << "                  PERPLEXITY: " << pow(2, (objective/log(2) / total_words)) << endl;
  }

  return 0;
}

