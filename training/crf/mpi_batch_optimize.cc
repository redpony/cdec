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

#include <boost/shared_ptr.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "sentence_metadata.h"
#include "cllh_observer.h"
#include "verbose.h"
#include "hg.h"
#include "prob.h"
#include "inside_outside.h"
#include "ff_register.h"
#include "decoder.h"
#include "filelib.h"
#include "stringlib.h"
#include "optimize.h"
#include "fdict.h"
#include "weights.h"
#include "sparse_vector.h"

using namespace std;
namespace po = boost::program_options;

bool InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("input_weights,w",po::value<string>(),"Input feature weights file")
        ("training_data,t",po::value<string>(),"Training data")
        ("test_data,T",po::value<string>(),"(optional) test data")
        ("decoder_config,c",po::value<string>(),"Decoder configuration file")
        ("output_weights,o",po::value<string>()->default_value("-"),"Output feature weights file")
        ("optimization_method,m", po::value<string>()->default_value("lbfgs"), "Optimization method (sgd, lbfgs, rprop)")
	("correction_buffers,M", po::value<int>()->default_value(10), "Number of gradients for LBFGS to maintain in memory")
        ("gaussian_prior,p","Use a Gaussian prior on the weights")
        ("sigma_squared", po::value<double>()->default_value(1.0), "Sigma squared term for spherical Gaussian prior")
        ("means,u", po::value<string>(), "(optional) file containing the means for Gaussian prior");
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
    for (SparseVector<prob_t>::const_iterator it = acc_grad.begin(); it != acc_grad.end(); ++it)
      (*g)[it->first] = it->second.as_float();
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
    acc_grad -= ref_exp;
    acc_obj += (cur_obj - log_ref_z);
    trg_words += smeta.GetReference().size();
  }

  virtual void NotifyDecodingComplete(const SentenceMetadata&) {
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

template <typename T>
struct VectorPlus : public binary_function<vector<T>, vector<T>, vector<T> >  {
  vector<T> operator()(const vector<int>& a, const vector<int>& b) const {
    assert(a.size() == b.size());
    vector<T> v(a.size());
    transform(a.begin(), a.end(), b.begin(), v.begin(), plus<T>()); 
    return v;
  } 
}; 

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

  // load cdec.ini and set up decoder
  vector<string> cdec_ini;
  ReadConfig(conf["decoder_config"].as<string>(), &cdec_ini);
  istringstream ini;
  StoreConfig(cdec_ini, &ini);
  if (rank == 0) cerr << "Loading grammar...\n";
  Decoder* decoder = new Decoder(&ini);
  if (decoder->GetConf()["input"].as<string>() != "-") {
    cerr << "cdec.ini must not set an input file\n";
    return 1;
  }
  if (rank == 0) cerr << "Done loading grammar!\n";

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

  const bool gaussian_prior = conf.count("gaussian_prior");
  vector<weight_t> means(num_feats, 0);
  if (conf.count("means")) {
    if (!gaussian_prior) {
      cerr << "Don't use --means without --gaussian_prior!\n";
      exit(1);
    }
    Weights::InitFromFile(conf["means"].as<string>(), &means);
  }
  boost::shared_ptr<BatchOptimizer> o;
  if (rank == 0) {
    const string omethod = conf["optimization_method"].as<string>();
    if (omethod == "rprop")
      o.reset(new RPropOptimizer(num_feats));  // TODO add configuration
    else
      o.reset(new LBFGSOptimizer(num_feats, conf["correction_buffers"].as<int>()));
    cerr << "Optimizer: " << o->Name() << endl;
  }
  double objective = 0;
  vector<double> gradient(num_feats, 0.0);
  vector<double> rcv_grad;
  rcv_grad.clear();
  bool converged = false;

  vector<string> corpus, test_corpus;
  ReadTrainingCorpus(conf["training_data"].as<string>(), rank, size, &corpus);
  assert(corpus.size() > 0);
  if (conf.count("test_data"))
    ReadTrainingCorpus(conf["test_data"].as<string>(), rank, size, &test_corpus);

  TrainingObserver observer;
  ConditionalLikelihoodObserver cllh_observer;
  while (!converged) {
    observer.Reset();
    cllh_observer.Reset();
#ifdef HAVE_MPI
    mpi::timer timer;
    world.barrier();
#endif
    if (rank == 0) {
      cerr << "Starting decoding... (~" << corpus.size() << " sentences / proc)\n";
      cerr << "  Testset size: " << test_corpus.size() << " sentences / proc)\n";
    }
    for (int i = 0; i < corpus.size(); ++i)
      decoder->Decode(corpus[i], &observer);
    cerr << "  process " << rank << '/' << size << " done\n";
    fill(gradient.begin(), gradient.end(), 0);
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
    if (rank == 0)
      cerr << "TRAINING CORPUS: ln p(f|e)=" << objective << "\t log_2 p(f|e) = " << (objective/log(2)) << "\t cond. entropy = " << (objective/log(2) / total_words) << "\t ppl = " << pow(2, (objective/log(2) / total_words)) << endl;

    for (int i = 0; i < test_corpus.size(); ++i)
      decoder->Decode(test_corpus[i], &cllh_observer);

    double test_objective = 0;
    unsigned test_total_words = 0;
#ifdef HAVE_MPI
    reduce(world, cllh_observer.acc_obj, test_objective, std::plus<double>(), 0);
    reduce(world, cllh_observer.trg_words, test_total_words, std::plus<unsigned>(), 0);
#else
    test_objective = cllh_observer.acc_obj;
    test_total_words = cllh_observer.trg_words;
#endif

    if (rank == 0) {  // run optimizer only on rank=0 node
      if (test_corpus.size())
        cerr << "    TEST CORPUS: ln p(f|e)=" << test_objective << "\t log_2 p(f|e) = " << (test_objective/log(2)) << "\t cond. entropy = " << (test_objective/log(2) / test_total_words) << "\t ppl = " << pow(2, (test_objective/log(2) / test_total_words)) << endl;
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
      vector<weight_t> old = lambdas;
      int c = 0;
      while (old == lambdas) {
        ++c;
        if (c > 1) { cerr << "Same lambdas, repeating optimization\n"; }
        o->Optimize(objective, gradient, &lambdas);
        assert(c < 5);
      }
      old.clear();
      Weights::SanityCheck(lambdas);
      Weights::ShowLargestFeatures(lambdas);

      converged = o->HasConverged();
      if (converged) { cerr << "OPTIMIZER REPORTS CONVERGENCE!\n"; }

      string fname = "weights.cur.gz";
      if (converged) { fname = "weights.final.gz"; }
      ostringstream vv;
      vv << "Objective = " << objective << "  (eval count=" << o->EvaluationCount() << ")";
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

