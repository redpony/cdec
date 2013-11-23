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

#include "stringlib.h"
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

bool InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("input_weights,w",po::value<string>(),"Input feature weights file")
        ("frozen_features,z",po::value<string>(), "List of features not to optimize")
        ("training_data,t",po::value<string>(),"Training data corpus")
        ("training_agenda,a",po::value<string>(), "Text file listing a series of configuration files and the number of iterations to train using each configuration successively")
        ("minibatch_size_per_proc,s", po::value<unsigned>()->default_value(5), "Number of training instances evaluated per processor in each minibatch")
        ("optimization_method,m", po::value<string>()->default_value("sgd"), "Optimization method (sgd)")
        ("max_walltime", po::value<unsigned>(), "Maximum walltime to run (in minutes)")
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

  if (conf->count("help") || !conf->count("training_data") || !conf->count("training_agenda")) {
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
    acc_grad.clear();
    acc_obj = 0;
    total_complete = 0;
  } 

  void SetLocalGradientAndObjective(vector<double>* g, double* o) const {
    *o = acc_obj;
    for (SparseVector<prob_t>::const_iterator it = acc_grad.begin(); it != acc_grad.end(); ++it)
      (*g)[it->first] = it->second.as_float();
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
    assert(!std::isnan(log_ref_z));
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
      g->set_value(it->first, it->second.as_float());
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

bool LoadAgenda(const string& file, vector<pair<string, int> >* a) {
  ReadFile rf(file);
  istream& in = *rf.stream();
  string line;
  while(in) {
    getline(in, line);
    if (!in) break;
    if (line.empty()) continue;
    if (line[0] == '#') continue;
    int sc = 0;
    if (line.size() < 3) return false;
    for (int i = 0; i < line.size(); ++i) { if (line[i] == ' ') ++sc; }
    if (sc != 1) { cerr << "Too many spaces in line: " << line << endl; return false; }
    size_t d = line.find(" ");
    pair<string, int> x;
    x.first = line.substr(0,d);
    x.second = atoi(line.substr(d+1).c_str());
    a->push_back(x);
    if (!FileExists(x.first)) {
      cerr << "Can't find file " << x.first << endl;
      return false;
    }
  }
  return true;
}

int main(int argc, char** argv) {
  cerr << "THIS SOFTWARE IS DEPRECATED YOU SHOULD USE mpi_flex_optimize\n";
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
  boost::shared_ptr<MT19937> rng;

  po::variables_map conf;
  if (!InitCommandLine(argc, argv, &conf))
    return 1;

  vector<pair<string, int> > agenda;
  if (!LoadAgenda(conf["training_agenda"].as<string>(), &agenda))
    return 1;
  if (rank == 0)
    cerr << "Loaded agenda defining " << agenda.size() << " training epochs\n";

  assert(agenda.size() > 0);

  if (1) {  // hack to load the feature hash functions -- TODO this should not be in cdec.ini
    const string& cur_config = agenda[0].first;
    const unsigned max_iteration = agenda[0].second;
    ReadFile ini_rf(cur_config);
    Decoder decoder(ini_rf.stream());
  }

  // load initial weights
  vector<weight_t> init_weights;
  if (conf.count("input_weights"))
    Weights::InitFromFile(conf["input_weights"].as<string>(), &init_weights);

  vector<int> frozen_fids;
  if (conf.count("frozen_features")) {
    ReadFile rf(conf["frozen_features"].as<string>());
    istream& in = *rf.stream();
    string line;
    while(in) {
      getline(in, line);
      if (line.empty()) continue;
      if (line[0] == ' ' || line[line.size() - 1] == ' ') { line = Trim(line); }
      frozen_fids.push_back(FD::Convert(line));
    }
    if (rank == 0) cerr << "Freezing " << frozen_fids.size() << " features.\n";
  }

  vector<string> corpus;
  vector<int> ids;
  ReadTrainingCorpus(conf["training_data"].as<string>(), rank, size, &corpus, &ids);
  assert(corpus.size() > 0);

  boost::shared_ptr<OnlineOptimizer> o;
  boost::shared_ptr<LearningRateSchedule> lr;

  const unsigned size_per_proc = conf["minibatch_size_per_proc"].as<unsigned>();
  if (size_per_proc > corpus.size()) {
    cerr << "Minibatch size must be smaller than corpus size!\n";
    return 1;
  }

  size_t total_corpus_size = 0;
#ifdef HAVE_MPI
  reduce(world, corpus.size(), total_corpus_size, std::plus<size_t>(), 0);
#else
  total_corpus_size = corpus.size();
#endif

  if (rank == 0) {
    cerr << "Total corpus size: " << total_corpus_size << endl;
    const unsigned batch_size = size_per_proc * size;
    // TODO config
    lr.reset(new ExponentialDecayLearningRate(batch_size, conf["eta_0"].as<double>()));

    const string omethod = conf["optimization_method"].as<string>();
    if (omethod == "sgd") {
      const double C = conf["regularization_strength"].as<double>();
      o.reset(new CumulativeL1OnlineOptimizer(lr, total_corpus_size, C, frozen_fids));
    } else {
      assert(!"fail");
    }
  }
  if (conf.count("random_seed"))
    rng.reset(new MT19937(conf["random_seed"].as<uint32_t>()));
  else
    rng.reset(new MT19937);

  SparseVector<double> x;
  Weights::InitSparseVector(init_weights, &x);
  TrainingObserver observer;

  int write_weights_every_ith = 100; // TODO configure
  int titer = -1;

  unsigned timeout = 0;
  if (conf.count("max_walltime")) timeout = 60 * conf["max_walltime"].as<unsigned>();
  const time_t start_time = time(NULL);
  for (int ai = 0; ai < agenda.size(); ++ai) {
    const string& cur_config = agenda[ai].first;
    const unsigned max_iteration = agenda[ai].second;
    if (rank == 0)
      cerr << "STARTING TRAINING EPOCH " << (ai+1) << ". CONFIG=" << cur_config << endl;
    // load cdec.ini and set up decoder
    ReadFile ini_rf(cur_config);
    Decoder decoder(ini_rf.stream());
    vector<weight_t>& lambdas = decoder.CurrentWeightVector();
    if (ai == 0) { lambdas.swap(init_weights); init_weights.clear(); }

    if (rank == 0)
      o->ResetEpoch(); // resets the learning rate-- TODO is this good?

    int iter = -1;
    bool converged = false;
    while (!converged) {
#ifdef HAVE_MPI
      mpi::timer timer;
#endif
      x.init_vector(&lambdas);
      ++iter; ++titer;
      observer.Reset();
      if (rank == 0) {
        converged = (iter == max_iteration);
        Weights::SanityCheck(lambdas);
        static int cc = 0; ++cc; if (cc > 1) { Weights::ShowLargestFeatures(lambdas); }
        string fname = "weights.cur.gz";
        if (iter % write_weights_every_ith == 0) {
          ostringstream o; o << "weights.epoch_" << (ai+1) << '.' << iter << ".gz";
          fname = o.str();
        }
        const time_t cur_time = time(NULL);
        if (timeout) {
          if ((cur_time - start_time) > timeout) converged = true;
        }
        if (converged && ((ai+1)==agenda.size())) { fname = "weights.final.gz"; }
        ostringstream vv;
        double minutes = (cur_time - start_time) / 60.0;
        vv << "total walltime=" << minutes << "min iter=" << titer << " (of current config iter=" << iter << ")  minibatch=" << size_per_proc << " sentences/proc x " << size << " procs.   num_feats=" << x.size() << '/' << FD::NumFeats() << "   passes_thru_data=" << (titer * size_per_proc / static_cast<double>(corpus.size())) << "   eta=" << lr->eta(titer);
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
        g /= (size_per_proc * size);
        o->UpdateWeights(g, FD::NumFeats(), &x);
      }
#ifdef HAVE_MPI
      broadcast(world, x, 0);
      broadcast(world, converged, 0);
      world.barrier();
      if (rank == 0) { cerr << "  ELAPSED TIME THIS ITERATION=" << timer.elapsed() << endl; }
#endif
    }
  }
  return 0;
}
