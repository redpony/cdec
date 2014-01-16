#include <sstream>
#include <iostream>
#include <fstream>
#include <vector>
#include <cassert>
#include <cmath>
#include <ctime>

#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/shared_ptr.hpp>

#include "config.h"
#include "stringlib.h"
#include "verbose.h"
#include "cllh_observer.h"
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

bool InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("weights,w",po::value<string>(), "Initial feature weights")
        ("training_data,d",po::value<string>(), "Training data corpus")
        ("test_data,t",po::value<string>(), "(optional) Test data")
        ("decoder_config,c",po::value<string>(), "Decoder configuration file")
        ("minibatch_size_per_proc,s", po::value<unsigned>()->default_value(8),
            "Number of training instances evaluated per processor in each minibatch")
        ("max_passes", po::value<double>()->default_value(20.0), "Maximum number of passes through the data")
        ("max_walltime", po::value<unsigned>(), "Walltime to run (in minutes)")
        ("write_every_n_minibatches", po::value<unsigned>()->default_value(100), "Write weights every N minibatches processed")
        ("random_seed,S", po::value<uint32_t>(), "Random seed")
        ("regularization,r", po::value<string>()->default_value("none"),
            "Regularization 'none', 'l1', or 'l2'")
        ("regularization_strength,C", po::value<double>(), "Regularization strength")
        ("eta,e", po::value<double>()->default_value(1.0), "Initial learning rate (eta)");
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

void ReadTrainingCorpus(const string& fname, int rank, int size, vector<string>* c, vector<int>* order) {
  ReadFile rf(fname);
  istream& in = *rf.stream();
  string line;
  int id = 0;
  while(getline(in, line)) {
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
    acc_grad.clear();
    acc_obj = 0;
    total_complete = 0;
  } 

  virtual void NotifyDecodingStart(const SentenceMetadata&) {
    cur_model_exp.clear();
    cur_obj = 0;
    state = 1;
  }

  // compute model expectations, denominator of objective
  virtual void NotifyTranslationForest(const SentenceMetadata&, Hypergraph* hg) {
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
  virtual void NotifyAlignmentForest(const SentenceMetadata&, Hypergraph* hg) {
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
    assert(!std::isnan(log_ref_z));
    ref_exp -= cur_model_exp;
    acc_grad += ref_exp;
    acc_obj += (cur_obj - log_ref_z);
  }

  virtual void NotifyDecodingComplete(const SentenceMetadata&) {
    if (state == 3) {
      ++total_complete;
    } else {
    }
  }

  void GetGradient(SparseVector<double>* g) const {
    g->clear();
#if HAVE_CXX11 && (__GNUC_MINOR__ > 4 || __GNUC__ > 4)
    for (auto& gi : acc_grad) {
#else
    for (FastSparseVector<prob_t>::const_iterator it = acc_grad.begin(); it != acc_grad.end(); ++it) {
      const pair<unsigned, prob_t>& gi = *it;
#endif
      g->set_value(gi.first, -gi.second.as_float());
    }
  }

  int total_complete;
  SparseVector<prob_t> cur_model_exp;
  SparseVector<prob_t> acc_grad;
  double acc_obj;
  double cur_obj;
  int state;
};

#ifdef HAVE_MPI
namespace boost { namespace mpi {
  template<>
  struct is_commutative<std::plus<SparseVector<double> >, SparseVector<double> > 
    : mpl::true_ { };
} } // end namespace boost::mpi
#endif

class AdaGradOptimizer {
 public:
  explicit AdaGradOptimizer(double e) :
      eta(e),
      G() {}
  void update(const SparseVector<double>& g, vector<double>* x, SparseVector<double>* sx) {
    if (x->size() > G.size()) G.resize(x->size(), 0.0);
#if HAVE_CXX11 && (__GNUC_MINOR__ > 4 || __GNUC__ > 4)
    for (auto& gi : g) {
#else
    for (SparseVector<double>::const_iterator it = g.begin(); it != g.end(); ++it) {
      const pair<unsigned,double>& gi = *it;
#endif
      if (gi.second) {
        G[gi.first] += gi.second * gi.second;
        (*x)[gi.first] -= eta / sqrt(G[gi.first]) * gi.second;
        sx->add_value(gi.first, -eta / sqrt(G[gi.first]) * gi.second);
      }
    }
  }
  const double eta;
  vector<double> G;
};

class AdaGradL1Optimizer {
 public:
  explicit AdaGradL1Optimizer(double e, double l) :
      t(),
      eta(e),
      lambda(l),
      G() {}
  void update(const SparseVector<double>& g, vector<double>* x, SparseVector<double>* sx) {
    t += 1.0;
    if (x->size() > G.size()) {
      G.resize(x->size(), 0.0);
      u.resize(x->size(), 0.0);
    }
#if HAVE_CXX11 && (__GNUC_MINOR__ > 4 || __GNUC__ > 4)
    for (auto& gi : g) {
#else
    for (SparseVector<double>::const_iterator it = g.begin(); it != g.end(); ++it) {
      const pair<unsigned,double>& gi = *it;
#endif
      if (gi.second) {
        u[gi.first] += gi.second;
        G[gi.first] += gi.second * gi.second;
        sx->set_value(gi.first, 1.0);  // this is a dummy value to trigger recomputation
      }
    }

    // compute updates (avoid invalidating iterators by putting them all
    // in the vector vupdate and applying them after this)
    vector<pair<unsigned, double>> vupdate;
#if HAVE_CXX11 && (__GNUC_MINOR__ > 4 || __GNUC__ > 4)
    for (auto& xi : *sx) {
#else
    for (SparseVector<double>::iterator it = sx->begin(); it != sx->end(); ++it) {
      const pair<unsigned,double>& xi = *it;
#endif
      double z = fabs(u[xi.first] / t) - lambda;
      double s = 1;
      if (u[xi.first] > 0) s = -1;
      if (z > 0 && G[xi.first]) {
        vupdate.push_back(make_pair(xi.first, eta * s * z * t / sqrt(G[xi.first])));
      } else {
        vupdate.push_back(make_pair(xi.first, 0.0));
      }
    }

    // apply updates
    for (unsigned i = 0; i < vupdate.size(); ++i) {
      if (vupdate[i].second) {
        sx->set_value(vupdate[i].first, vupdate[i].second);
        (*x)[vupdate[i].first] = vupdate[i].second;
      } else {
        (*x)[vupdate[i].first] = 0.0;
        sx->erase(vupdate[i].first);
      }
    }
  }
  double t;
  const double eta;
  const double lambda;
  vector<double> G, u;
};

unsigned non_zeros(const vector<double>& x) {
  unsigned nz = 0;
  for (unsigned i = 0; i < x.size(); ++i)
    if (x[i]) ++nz;
  return nz;
}

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

  ReadFile ini_rf(conf["decoder_config"].as<string>());
  Decoder decoder(ini_rf.stream());

  // load initial weights
  vector<weight_t> init_weights;
  if (conf.count("input_weights"))
    Weights::InitFromFile(conf["input_weights"].as<string>(), &init_weights);

  vector<string> corpus, test_corpus;
  vector<int> ids;
  ReadTrainingCorpus(conf["training_data"].as<string>(), rank, size, &corpus, &ids);
  assert(corpus.size() > 0);
  if (conf.count("test_data"))
    ReadTrainingCorpus(conf["test_data"].as<string>(), rank, size, &corpus, &ids);

  const unsigned size_per_proc = conf["minibatch_size_per_proc"].as<unsigned>();
  if (size_per_proc > corpus.size()) {
    cerr << "Minibatch size must be smaller than corpus size!\n";
    return 1;
  }
  const double minibatch_size = size_per_proc * size;

  size_t total_corpus_size = 0;
#ifdef HAVE_MPI
  reduce(world, corpus.size(), total_corpus_size, std::plus<size_t>(), 0);
#else
  total_corpus_size = corpus.size();
#endif

  if (rank == 0)
    cerr << "Total corpus size: " << total_corpus_size << endl;

  boost::shared_ptr<MT19937> rng;
  if (conf.count("random_seed"))
    rng.reset(new MT19937(conf["random_seed"].as<uint32_t>()));
  else
    rng.reset(new MT19937);

  double passes_per_minibatch = static_cast<double>(size_per_proc) / total_corpus_size;

  int write_weights_every_ith = conf["write_every_n_minibatches"].as<unsigned>();

  unsigned max_iteration = conf["max_passes"].as<double>() / passes_per_minibatch;
  ++max_iteration;
  if (rank == 0)
    cerr << "Max passes through data = " << conf["max_passes"].as<double>() << endl
         << "    --> max minibatches = " << max_iteration << endl;
  unsigned timeout = 0;
  if (conf.count("max_walltime"))
    timeout = 60 * conf["max_walltime"].as<unsigned>();
  vector<weight_t>& lambdas = decoder.CurrentWeightVector();
  if (init_weights.size()) {
    lambdas.swap(init_weights);
    init_weights.clear();
  }
  SparseVector<double> lambdas_sparse;
  Weights::InitSparseVector(lambdas, &lambdas_sparse);

  //AdaGradOptimizer adagrad(conf["eta"].as<double>());
  AdaGradL1Optimizer adagrad(conf["eta"].as<double>(), conf["regularization_strength"].as<double>());
  int iter = -1;
  bool converged = false;

  TrainingObserver observer;
  ConditionalLikelihoodObserver cllh_observer;

  const time_t start_time = time(NULL);
  while (!converged) {
#ifdef HAVE_MPI
      mpi::timer timer;
#endif
      ++iter;
      if (iter > 1) {
        lambdas_sparse.init_vector(&lambdas);
        if (rank == 0) {
          Weights::SanityCheck(lambdas);
          Weights::ShowLargestFeatures(lambdas);
        }
      }
      observer.Reset();
      if (rank == 0) {
        converged = (iter == max_iteration);
        string fname = "weights.cur.gz";
        if (iter % write_weights_every_ith == 0) {
          ostringstream o; o << "weights." << iter << ".gz";
          fname = o.str();
        }
        const time_t cur_time = time(NULL);
        if (timeout && ((cur_time - start_time) > timeout)) {
          converged = true;
          fname = "weights.final.gz";
        }
        ostringstream vv;
        double minutes = (cur_time - start_time) / 60.0;
        vv << "total walltime=" << minutes << " min  iter=" << iter << "  minibatch=" << size_per_proc << " sentences/proc x " << size << " procs.   num_feats=" << non_zeros(lambdas) << '/' << FD::NumFeats() << "   passes_thru_data=" << (iter * size_per_proc / static_cast<double>(corpus.size()));
        const string svv = vv.str();
        cerr << svv << endl;
        Weights::WriteToFile(fname, lambdas, true, &svv);
      }

      for (int i = 0; i < size_per_proc; ++i) {
        int ei = corpus.size() * rng->next();
        int id = ids[ei];
        decoder.SetId(id);
        decoder.Decode(corpus[ei], &observer);
      }
      SparseVector<double> local_grad, g;
      observer.GetGradient(&local_grad);
#ifdef HAVE_MPI
      reduce(world, local_grad, g, std::plus<SparseVector<double> >(), 0);
#else
      g.swap(local_grad);
#endif
      local_grad.clear();
      if (rank == 0) {
        g /= minibatch_size;
        lambdas.resize(FD::NumFeats(), 0.0); // might have seen new features
        adagrad.update(g, &lambdas, &lambdas_sparse);
      }
#ifdef HAVE_MPI
      broadcast(world, lambdas_sparse, 0);
      broadcast(world, converged, 0);
      world.barrier();
      if (rank == 0) { cerr << "  ELAPSED TIME THIS ITERATION=" << timer.elapsed() << endl; }
#endif
  }
  cerr << "CONVERGED = " << converged << endl;
  cerr << "EXITING...\n";
  return 0;
}

