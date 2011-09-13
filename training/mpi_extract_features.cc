#include <iostream>
#include <sstream>
#include <vector>
#include <cassert>

#include "config.h"
#ifdef HAVE_MPI
#include <boost/mpi.hpp>
#endif
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "ff_register.h"
#include "verbose.h"
#include "filelib.h"
#include "fdict.h"
#include "decoder.h"
#include "weights.h"

using namespace std;
namespace po = boost::program_options;

bool InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("training_data,t",po::value<string>(),"Training data corpus")
        ("decoder_config,c",po::value<string>(),"Decoder configuration file")
        ("weights,w", po::value<string>(), "(Optional) weights file; weights may affect what features are encountered in pruning configurations")
        ("output_prefix,o",po::value<string>()->default_value("features"),"Output path prefix");
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
    cerr << "Decode an input set (optionally in parallel using MPI) and write\nout the feature strings encountered.\n";
    cerr << dcmdline_options << endl;
    return false;
  }
  return true;
}

void ReadTrainingCorpus(const string& fname, int rank, int size, vector<string>* c) {
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

struct TrainingObserver : public DecoderObserver {

  virtual void NotifyDecodingStart(const SentenceMetadata&) {
  }

  // compute model expectations, denominator of objective
  virtual void NotifyTranslationForest(const SentenceMetadata&, Hypergraph* hg) {
  }

  // compute "empirical" expectations, numerator of objective
  virtual void NotifyAlignmentForest(const SentenceMetadata& smeta, Hypergraph* hg) {
  }
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

  if (FD::UsingPerfectHashFunction()) {
    cerr << "Your configuration file has enabled a cmph hash function. Please disable.\n";
    return 1;
  }

  // load optional weights
  if (conf.count("weights"))
    Weights::InitFromFile(conf["weights"].as<string>(), &decoder.CurrentWeightVector());

  vector<string> corpus;
  ReadTrainingCorpus(conf["training_data"].as<string>(), rank, size, &corpus);
  assert(corpus.size() > 0);

  TrainingObserver observer;

  if (rank == 0)
    cerr << "Each processor is decoding ~" << corpus.size() << " training examples...\n";

  for (int i = 0; i < corpus.size(); ++i)
    decoder.Decode(corpus[i], &observer);

  {
    ostringstream os;
    os << conf["output_prefix"].as<string>() << '.' << rank << "_of_" << size;
    WriteFile wf(os.str());
    ostream& out = *wf.stream();
    const unsigned num_feats = FD::NumFeats();
    for (unsigned i = 1; i < num_feats; ++i) {
      out << FD::Convert(i) << endl;
    }
    cerr << "Wrote " << os.str() << endl;
  }

#ifdef HAVE_MPI
  world.barrier();
#else
#endif

  return 0;
}

