#include <sstream>
#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>

#include "config.h"
#ifdef HAVE_MPI
#include <boost/mpi/timer.hpp>
#include <boost/mpi.hpp>
namespace mpi = boost::mpi;
#endif

#include <boost/unordered_map.hpp>
#include <boost/functional/hash.hpp>
#include <boost/shared_ptr.hpp>
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
#include "stringlib.h"
#include "fdict.h"
#include "weights.h"
#include "sparse_vector.h"

using namespace std;
namespace po = boost::program_options;

bool InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("input_weights,w",po::value<string>(),"Input feature weights file")
        ("iterations,n",po::value<unsigned>()->default_value(50), "Number of training iterations")
        ("training_data,t",po::value<string>(),"Training data")
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

  if (conf->count("help") || !conf->count("input_weights") || !(conf->count("training_data")) || !conf->count("decoder_config")) {
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
  void Reset() {
    acc_grad.clear();
    acc_obj = 0;
    total_complete = 0;
    trg_words = 0;
  } 

  void SetLocalGradientAndObjective(vector<double>* g, double* o) const {
    *o = acc_obj;
    for (SparseVector<double>::const_iterator it = acc_grad.begin(); it != acc_grad.end(); ++it)
      (*g)[it->first] = it->second;
  }

  virtual void NotifyDecodingStart(const SentenceMetadata& smeta) {
    state = 1;
  }

  // compute model expectations, denominator of objective
  virtual void NotifyTranslationForest(const SentenceMetadata& smeta, Hypergraph* hg) {
    assert(state == 1);
    trg_words += smeta.GetSourceLength();
    state = 2;
    SparseVector<prob_t> exps;
    const prob_t z = InsideOutside<prob_t,
                                   EdgeProb,
                                   SparseVector<prob_t>,
                                   EdgeFeaturesAndProbWeightFunction>(*hg, &exps);
    exps /= z;
    for (SparseVector<prob_t>::iterator it = exps.begin(); it != exps.end(); ++it)
      acc_grad.add_value(it->first, it->second.as_float());

    acc_obj += log(z);
  }

  // compute "empirical" expectations, numerator of objective
  virtual void NotifyAlignmentForest(const SentenceMetadata& smeta, Hypergraph* hg) {
    cerr << "Shouldn't get an alignment forest!\n";
    abort();
  }

  virtual void NotifyDecodingComplete(const SentenceMetadata& smeta) {
    ++total_complete;
  }

  int total_complete;
  SparseVector<double> acc_grad;
  double acc_obj;
  unsigned trg_words;
  int state;
};

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

#if 0
template <typename T>
struct VectorPlus : public binary_function<vector<T>, vector<T>, vector<T> >  {
  vector<T> operator()(const vector<int>& a, const vector<int>& b) const {
    assert(a.size() == b.size());
    vector<T> v(a.size());
    transform(a.begin(), a.end(), b.begin(), v.begin(), plus<T>()); 
    return v;
  } 
}; 
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
  SetSilent(true);  // turn off verbose decoder output
  register_feature_functions();

  po::variables_map conf;
  if (!InitCommandLine(argc, argv, &conf)) return 1;
  const unsigned iterations = conf["iterations"].as<unsigned>();

  // load cdec.ini and set up decoder
  vector<string> cdec_ini;
  ReadConfig(conf["decoder_config"].as<string>(), &cdec_ini);
  istringstream ini;
  StoreConfig(cdec_ini, &ini);
  Decoder* decoder = new Decoder(&ini);
  if (decoder->GetConf()["input"].as<string>() != "-") {
    cerr << "cdec.ini must not set an input file\n";
    return 1;
  }

  // load initial weights
  if (rank == 0) { cerr << "Loading weights...\n"; }
  vector<weight_t>& lambdas = decoder->CurrentWeightVector();
  Weights::InitFromFile(conf["input_weights"].as<string>(), &lambdas);
  if (rank == 0) { cerr << "Done loading weights.\n"; }

  // freeze feature set (should be optional?)
  const bool freeze_feature_set = true;
  if (freeze_feature_set) FD::Freeze();

  const int num_feats = FD::NumFeats();
  if (rank == 0) cerr << "Number of features: " << num_feats << endl;
  lambdas.resize(num_feats);

  vector<double> gradient(num_feats, 0.0);
  vector<double> rcv_grad;
  rcv_grad.clear();
  bool converged = false;

  vector<string> corpus, test_corpus;
  ReadTrainingCorpus(conf["training_data"].as<string>(), rank, size, &corpus);
  assert(corpus.size() > 0);
  if (conf.count("test_data"))
    ReadTrainingCorpus(conf["test_data"].as<string>(), rank, size, &test_corpus);

  // build map from feature id to the accumulator that should normalize
  boost::unordered_map<std::string, boost::unordered_map<int, double>, boost::hash<std::string> > ccs;
  vector<boost::unordered_map<int, double>* > cpd_to_acc;
  if (rank == 0) {
    cpd_to_acc.resize(num_feats);
    for (unsigned f = 1; f < num_feats; ++f) {
      string normalizer;
      //0 ||| 7 9 ||| Bi:BOS_7=1 Bi:7_9=1 Bi:9_EOS=1 Id:a:7=1 Uni:7=1 Id:b:9=1 Uni:9=1 ||| 0
      const string& fstr = FD::Convert(f);
      if (fstr.find("Bi:") == 0) {
        size_t pos = fstr.rfind('_');
        if (pos < fstr.size())
          normalizer = fstr.substr(0, pos);
      } else if (fstr.find("Id:") == 0) {
        size_t pos = fstr.rfind(':');
        if (pos < fstr.size()) {
          normalizer = "Emit:";
          normalizer += fstr.substr(pos);
        }
      }
      if (normalizer.size() > 0) {
        boost::unordered_map<int, double>& acc = ccs[normalizer];
        cpd_to_acc[f] = &acc;
      }
    }
  }

  TrainingObserver observer;
  int iteration = 0;
  while (!converged) {
    ++iteration;
    observer.Reset();
#ifdef HAVE_MPI
    mpi::timer timer;
    world.barrier();
#endif
    if (rank == 0) {
      cerr << "Starting decoding... (~" << corpus.size() << " sentences / proc)\n";
      cerr << "  Testset size: " << test_corpus.size() << " sentences / proc)\n";
      for(boost::unordered_map<string, boost::unordered_map<int,double>, boost::hash<string> >::iterator it = ccs.begin(); it != ccs.end(); ++it)
        it->second.clear();
    }
    for (int i = 0; i < corpus.size(); ++i)
      decoder->Decode(corpus[i], &observer);
    cerr << "  process " << rank << '/' << size << " done\n";
    fill(gradient.begin(), gradient.end(), 0);
    double objective = 0;
    observer.SetLocalGradientAndObjective(&gradient, &objective);

    unsigned total_words = 0;
#ifdef HAVE_MPI
    double to = 0;
    rcv_grad.resize(num_feats, 0.0);
    mpi::reduce(world, &gradient[0], gradient.size(), &rcv_grad[0], plus<double>(), 0);
    swap(gradient, rcv_grad);
    rcv_grad.clear();

    reduce(world, observer.trg_words, total_words, std::plus<unsigned>(), 0);
    mpi::reduce(world, objective, to, plus<double>(), 0);
    objective = to;
#else
    total_words = observer.trg_words;
#endif
    if (rank == 0) {  // run optimizer only on rank=0 node
      cerr << "TRAINING CORPUS: ln p(x)=" << objective << "\t log_2 p(x) = " << (objective/log(2)) << "\t cross entropy = " << (objective/log(2) / total_words) << "\t ppl = " << pow(2, (-objective/log(2) / total_words)) << endl;
      for (unsigned f = 1; f < num_feats; ++f) {
        boost::unordered_map<int, double>* m = cpd_to_acc[f];
        if (m && gradient[f]) {
          (*m)[f] += gradient[f];
        }
        for(boost::unordered_map<string, boost::unordered_map<int,double>, boost::hash<string> >::iterator it = ccs.begin(); it != ccs.end(); ++it) {
          const boost::unordered_map<int,double>& ccs = it->second;
          double z = 0;
          for (boost::unordered_map<int,double>::const_iterator ci = ccs.begin(); ci != ccs.end(); ++ci)
            z += ci->second + 1e-09;
          double lz = log(z);
          for (boost::unordered_map<int,double>::const_iterator ci = ccs.begin(); ci != ccs.end(); ++ci)
            lambdas[ci->first] = log(ci->second + 1e-09) - lz;
        }
      }
      Weights::SanityCheck(lambdas);
      Weights::ShowLargestFeatures(lambdas);

      converged = (iteration == iterations);

      string fname = "weights.cur.gz";
      if (converged) { fname = "weights.final.gz"; }
      ostringstream vv;
      vv << "Objective = " << objective << "  (eval count=" << iteration << ")";
      const string svv = vv.str();
      Weights::WriteToFile(fname, lambdas, true, &svv);
    }  // rank == 0
    int cint = converged;
#ifdef HAVE_MPI
    mpi::broadcast(world, &lambdas[0], lambdas.size(), 0);
    mpi::broadcast(world, cint, 0);
    if (rank == 0) { cerr << "  ELAPSED TIME THIS ITERATION=" << timer.elapsed() << endl; }
#endif
    converged = cint;
  }
  return 0;
}

