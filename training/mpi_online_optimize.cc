#include <sstream>
#include <iostream>
#include <fstream>
#include <vector>
#include <cassert>
#include <cmath>

#include <mpi.h>
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
        ("eta_0,e", po::value<double>()->default_value(0.2), "Initial learning rate for SGD (eta_0)")
        ("L1,1","Use L1 regularization")
        ("gaussian_prior,g","Use a Gaussian prior on the weights")
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

  if (conf->count("help") || !conf->count("input_weights") || !conf->count("training_data") || !conf->count("decoder_config")) {
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
    acc_grad -= ref_exp;
    acc_obj += (cur_obj - log_ref_z);
  }

  virtual void NotifyDecodingComplete(const SentenceMetadata& smeta) {
    if (state == 3) {
      ++total_complete;
    } else {
    }
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

int main(int argc, char** argv) {
  MPI::Init(argc, argv);
  const int size = MPI::COMM_WORLD.Get_size(); 
  const int rank = MPI::COMM_WORLD.Get_rank();
  SetSilent(true);  // turn off verbose decoder output
  cerr << "MPI: I am " << rank << '/' << size << endl;
  register_feature_functions();
  MT19937* rng = NULL;
  if (rank == 0) rng = new MT19937;

  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);

  // load initial weights
  Weights weights;
  weights.InitFromFile(conf["input_weights"].as<string>());

  // freeze feature set
  const bool freeze_feature_set = conf.count("freeze_feature_set");
  if (freeze_feature_set) FD::Freeze();

  // load cdec.ini and set up decoder
  ReadFile ini_rf(conf["decoder_config"].as<string>());
  Decoder decoder(ini_rf.stream());
  if (decoder.GetConf()["input"].as<string>() != "-") {
    cerr << "cdec.ini must not set an input file\n";
    MPI::COMM_WORLD.Abort(1);
  }

  vector<string> corpus;
  ReadTrainingCorpus(conf["training_data"].as<string>(), &corpus);
  assert(corpus.size() > 0);

  std::tr1::shared_ptr<OnlineOptimizer> o;
  std::tr1::shared_ptr<LearningRateSchedule> lr;
  if (rank == 0) {
    // TODO config
    lr.reset(new ExponentialDecayLearningRate(corpus.size(), conf["eta_0"].as<double>()));

    const string omethod = conf["optimization_method"].as<string>();
    if (omethod == "sgd") {
      const double C = 1.0;
      o.reset(new CumulativeL1OnlineOptimizer(lr, corpus.size(), C));
    } else {
      assert(!"fail");
    }
  }
  double objective = 0;
  vector<double> lambdas;
  weights.InitVector(&lambdas);
  bool converged = false;

  TrainingObserver observer;
  while (!converged) {
    observer.Reset();
    if (rank == 0) {
      cerr << "Starting decoding... (~" << corpus.size() << " sentences / proc)\n";
    }
    decoder.SetWeights(lambdas);
#if 0
    for (int i = 0; i < corpus.size(); ++i)
      decoder.Decode(corpus[i], &observer);

    fill(gradient.begin(), gradient.end(), 0);
    fill(rcv_grad.begin(), rcv_grad.end(), 0);
    observer.SetLocalGradientAndObjective(&gradient, &objective);

    double to = 0;
    MPI::COMM_WORLD.Reduce(const_cast<double*>(&gradient.data()[0]), &rcv_grad[0], num_feats, MPI::DOUBLE, MPI::SUM, 0);
    MPI::COMM_WORLD.Reduce(&objective, &to, 1, MPI::DOUBLE, MPI::SUM, 0);
    swap(gradient, rcv_grad);
    objective = to;

    if (rank == 0) {  // run optimizer only on rank=0 node
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
      weights.InitFromVector(lambdas);

      converged = o->HasConverged();
      if (converged) { cerr << "OPTIMIZER REPORTS CONVERGENCE!\n"; }

      string fname = "weights.cur.gz";
      if (converged) { fname = "weights.final.gz"; }
      ostringstream vv;
      vv << "Objective = " << objective << "  (eval count=" << o->EvaluationCount() << ")";
      const string svv = vv.str();
      weights.WriteToFile(fname, true, &svv);
    }  // rank == 0
    int cint = converged;
    MPI::COMM_WORLD.Bcast(const_cast<double*>(&lambdas.data()[0]), num_feats, MPI::DOUBLE, 0);
    MPI::COMM_WORLD.Bcast(&cint, 1, MPI::INT, 0);
    MPI::COMM_WORLD.Barrier();
    converged = cint;
#endif
  }
  MPI::Finalize(); 
  return 0;
}
