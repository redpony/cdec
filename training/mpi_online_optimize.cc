#include <sstream>
#include <iostream>
#include <fstream>
#include <vector>
#include <cassert>
#include <cmath>

#include <mpi.h>
#include <boost/mpi.hpp>
#include <boost/shared_ptr.hpp>
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

using namespace std;
using boost::shared_ptr;
namespace po = boost::program_options;

void SanityCheck(const vector<double>& w) {
  for (int i = 0; i < w.size(); ++i) {
    assert(!isnan(w[i]));
    assert(!isinf(w[i]));
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
        ("input_weights,w",po::value<string>(),"Input feature weights file")
        ("training_data,t",po::value<string>(),"Training data corpus")
        ("decoder_config,c",po::value<string>(),"Decoder configuration file")
        ("output_weights,o",po::value<string>()->default_value("-"),"Output feature weights file")
        ("minibatch_size_per_proc,s", po::value<unsigned>()->default_value(5), "Number of training instances evaluated per processor in each minibatch")
        ("freeze_feature_set,Z", "The feature set specified in the initial weights file is frozen throughout the duration of training")
        ("optimization_method,m", po::value<string>()->default_value("sgd"), "Optimization method (sgd)")
        ("fully_random,r", "Fully random draws from the training corpus")
        ("random_seed,S", po::value<uint32_t>(), "Random seed (if not specified, /dev/random will be used)")
        ("eta_0,e", po::value<double>()->default_value(0.2), "Initial learning rate for SGD (eta_0)")
        ("L1,1","Use L1 regularization")
        ("regularization_strength,C", po::value<double>()->default_value(1.0), "Regularization strength (C)");
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

void ReadTrainingCorpus(const string& fname, vector<string>* c) {
  ReadFile rf(fname);
  istream& in = *rf.stream();
  string line;
  while(in) {
    getline(in, line);
    if (!in) break;
    c->push_back(line);
  }
}

static const double kMINUS_EPSILON = -1e-6;

struct TrainingObserver : public DecoderObserver {
  void Reset() {
    acc_grad.clear();
    acc_obj = 0;
    total_complete = 0;
  } 

  void SetLocalGradientAndObjective(vector<double>* g, double* o) const {
    *o = acc_obj;
    for (SparseVector<prob_t>::const_iterator it = acc_grad.begin(); it != acc_grad.end(); ++it)
      (*g)[it->first] = it->second;
  }

  virtual void NotifyDecodingStart(const SentenceMetadata& smeta) {
    cur_model_exp.clear();
    cur_obj = 0;
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
    cur_obj = log(z);
    cur_model_exp /= z;
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
    ref_exp /= ref_z;

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
    ref_exp -= cur_model_exp;
    acc_grad += ref_exp;
    acc_obj += (cur_obj - log_ref_z);
  }

  virtual void NotifyDecodingComplete(const SentenceMetadata& smeta) {
    if (state == 3) {
      ++total_complete;
    } else {
    }
  }

  void GetGradient(SparseVector<double>* g) const {
    g->clear();
    for (SparseVector<prob_t>::const_iterator it = acc_grad.begin(); it != acc_grad.end(); ++it)
      g->set_value(it->first, it->second);
  }

  int total_complete;
  SparseVector<prob_t> cur_model_exp;
  SparseVector<prob_t> acc_grad;
  double acc_obj;
  double cur_obj;
  int state;
};

template <typename T>
inline void Shuffle(vector<T>* c, MT19937* rng) {
  unsigned size = c->size();
  for (unsigned i = size - 1; i > 0; --i) {
    const unsigned j = static_cast<unsigned>(rng->next() * i);
    swap((*c)[j], (*c)[i]);
  }
}

namespace mpi = boost::mpi;

namespace boost { namespace mpi {
  template<>
  struct is_commutative<std::plus<SparseVector<double> >, SparseVector<double> > 
    : mpl::true_ { };
} } // end namespace boost::mpi


int main(int argc, char** argv) {
  mpi::environment env(argc, argv);
  mpi::communicator world;
  const int size = world.size(); 
  const int rank = world.rank();
  SetSilent(true);  // turn off verbose decoder output
  cerr << "MPI: I am " << rank << '/' << size << endl;
  register_feature_functions();
  MT19937* rng = NULL;

  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);

  // load initial weights
  Weights weights;
  if (conf.count("input_weights"))
    weights.InitFromFile(conf["input_weights"].as<string>());

  // freeze feature set
  const bool freeze_feature_set = conf.count("freeze_feature_set");
  if (freeze_feature_set) FD::Freeze();

  // load cdec.ini and set up decoder
  ReadFile ini_rf(conf["decoder_config"].as<string>());
  Decoder decoder(ini_rf.stream());
  if (decoder.GetConf()["input"].as<string>() != "-") {
    cerr << "cdec.ini must not set an input file\n";
    abort();
  }

  vector<string> corpus;
  ReadTrainingCorpus(conf["training_data"].as<string>(), &corpus);
  assert(corpus.size() > 0);

  std::tr1::shared_ptr<OnlineOptimizer> o;
  std::tr1::shared_ptr<LearningRateSchedule> lr;
  vector<int> order(corpus.size());

  const bool fully_random = conf.count("fully_random");
  const unsigned size_per_proc = conf["minibatch_size_per_proc"].as<unsigned>();
  const unsigned batch_size = size_per_proc * size;
  if (rank == 0) {
    cerr << "Corpus: " << corpus.size() << "  batch size: " << batch_size << endl;
    if (batch_size > corpus.size()) {
      cerr << "  Reduce minibatch_size_per_proc!";
      abort();
    }

    // TODO config
    lr.reset(new ExponentialDecayLearningRate(batch_size, conf["eta_0"].as<double>()));

    const string omethod = conf["optimization_method"].as<string>();
    if (omethod == "sgd") {
      const double C = conf["regularization_strength"].as<double>();
      o.reset(new CumulativeL1OnlineOptimizer(lr, corpus.size(), C));
    } else {
      assert(!"fail");
    }

    for (unsigned i = 0; i < order.size(); ++i) order[i]=i;
    // randomize corpus
    if (conf.count("random_seed"))
      rng = new MT19937(conf["random_seed"].as<uint32_t>());
    else
      rng = new MT19937;
  }
  SparseVector<double> x;
  int miter = corpus.size();  // hack to cause initial broadcast of order info
  TrainingObserver observer;
  double objective = 0;
  bool converged = false;

  int iter = -1;
  vector<double> lambdas;
  while (!converged) {
    weights.InitFromVector(x);
    weights.InitVector(&lambdas);
    ++miter; ++iter;
    observer.Reset();
    decoder.SetWeights(lambdas);
    if (rank == 0) {
      SanityCheck(lambdas);
      ShowLargestFeatures(lambdas);
      string fname = "weights.cur.gz";
      if (converged) { fname = "weights.final.gz"; }
      ostringstream vv;
      vv << "Objective = " << objective; // << "  (eval count=" << o->EvaluationCount() << ")";
      const string svv = vv.str();
      weights.WriteToFile(fname, true, &svv);
    }

    if (fully_random || size * size_per_proc * miter > corpus.size()) {
      if (rank == 0)
        Shuffle(&order, rng);
      miter = 0;
      broadcast(world, order, 0);
    }
    if (rank == 0)
      cerr << "Starting decoding. minibatch=" << size_per_proc << " sentences/proc x " << size << " procs. num_feats=" << x.size() << " training data proc. = " << (iter * batch_size / static_cast<double>(corpus.size())) << "  eta=" << lr->eta(iter) << endl;

    const int beg = size * miter * size_per_proc + rank * size_per_proc;
    const int end = beg + size_per_proc;
    for (int i = beg; i < end; ++i) {
      int ex_num = order[i % order.size()];
      if (rank ==0 && size < 3) cerr << rank << ": ex_num=" << ex_num << endl;
      decoder.SetId(ex_num);
      decoder.Decode(corpus[ex_num], &observer);
    }
    SparseVector<double> local_grad, g;
    observer.GetGradient(&local_grad);
    reduce(world, local_grad, g, std::plus<SparseVector<double> >(), 0);
    if (rank == 0) {
      g /= batch_size;
      o->UpdateWeights(g, FD::NumFeats(), &x);
    }
    broadcast(world, x, 0);
    world.barrier();
  }
  return 0;
}
