#include <sstream>
#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>

#ifdef HAVE_MPI
#include <mpi.h>
#endif

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
#include "optimize.h"
#include "fdict.h"
#include "weights.h"
#include "sparse_vector.h"

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
        ("training_data,t",po::value<string>(),"Training data")
        ("decoder_config,d",po::value<string>(),"Decoder configuration file")
        ("sharded_input,s",po::value<string>(), "Corpus and grammar files are 'sharded' so each processor loads its own input and grammar file. Argument is the directory containing the shards.")
        ("output_weights,o",po::value<string>()->default_value("-"),"Output feature weights file")
        ("optimization_method,m", po::value<string>()->default_value("lbfgs"), "Optimization method (sgd, lbfgs, rprop)")
	("correction_buffers,M", po::value<int>()->default_value(10), "Number of gradients for LBFGS to maintain in memory")
        ("gaussian_prior,p","Use a Gaussian prior on the weights")
        ("means,u", po::value<string>(), "File containing the means for Gaussian prior")
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

  if (conf->count("help") || !conf->count("input_weights") || !(conf->count("training_data") | conf->count("sharded_input")) || !conf->count("decoder_config")) {
    cerr << dcmdline_options << endl;
#ifdef HAVE_MPI
    MPI::Finalize();
#endif
    exit(1);
  }
  if (conf->count("training_data") && conf->count("sharded_input")) {
    cerr << "Cannot specify both --training_data and --sharded_input\n";
#ifdef HAVE_MPI
    MPI::Finalize();
#endif
    exit(1);
  }
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

int main(int argc, char** argv) {
#ifdef HAVE_MPI
  MPI::Init(argc, argv);
  const int size = MPI::COMM_WORLD.Get_size(); 
  const int rank = MPI::COMM_WORLD.Get_rank();
#else
  const int size = 1;
  const int rank = 0;
#endif
  SetSilent(true);  // turn off verbose decoder output
  register_feature_functions();

  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);

  string shard_dir;
  if (conf.count("sharded_input")) {
    shard_dir = conf["sharded_input"].as<string>();
    if (!DirectoryExists(shard_dir)) {
      if (rank == 0) cerr << "Can't find shard directory: " << shard_dir << endl;
#ifdef HAVE_MPI
      MPI::Finalize();
#endif
      return 1;
    }
    if (rank == 0)
      cerr << "Shard directory: " << shard_dir << endl;
  }

  // load initial weights
  Weights weights;
  if (rank == 0) { cerr << "Loading weights...\n"; }
  weights.InitFromFile(conf["input_weights"].as<string>());
  if (rank == 0) { cerr << "Done loading weights.\n"; }

  // freeze feature set (should be optional?)
  const bool freeze_feature_set = true;
  if (freeze_feature_set) FD::Freeze();

  // load cdec.ini and set up decoder
  vector<string> cdec_ini;
  ReadConfig(conf["decoder_config"].as<string>(), &cdec_ini);
  if (shard_dir.size()) {
    if (rank == 0) {
      for (int i = 0; i < cdec_ini.size(); ++i) {
        if (cdec_ini[i].find("grammar=") == 0) {
          cerr << "!!! using sharded input and " << conf["decoder_config"].as<string>() << " contains a grammar specification:\n" << cdec_ini[i] << "\n  VERIFY THAT THIS IS CORRECT!\n";
        }
      }
    }
    ostringstream g;
    g << "grammar=" << shard_dir << "/grammar." << rank << "_of_" << size << ".gz";
    cdec_ini.push_back(g.str());
  }
  istringstream ini;
  StoreConfig(cdec_ini, &ini);
  if (rank == 0) cerr << "Loading grammar...\n";
  Decoder* decoder = new Decoder(&ini);
  if (decoder->GetConf()["input"].as<string>() != "-") {
    cerr << "cdec.ini must not set an input file\n";
#ifdef HAVE_MPI
    MPI::COMM_WORLD.Abort(1);
#endif
  }
  if (rank == 0) cerr << "Done loading grammar!\n";

  const int num_feats = FD::NumFeats();
  if (rank == 0) cerr << "Number of features: " << num_feats << endl;
  const bool gaussian_prior = conf.count("gaussian_prior");
  vector<double> means(num_feats, 0);
  if (conf.count("means")) {
    if (!gaussian_prior) {
      cerr << "Don't use --means without --gaussian_prior!\n";
      exit(1);
    }
    Weights wm; 
    wm.InitFromFile(conf["means"].as<string>());
    if (num_feats != FD::NumFeats()) {
      cerr << "[ERROR] Means file had unexpected features!\n";
      exit(1);
    }
    wm.InitVector(&means);
  }
  shared_ptr<BatchOptimizer> o;
  if (rank == 0) {
    const string omethod = conf["optimization_method"].as<string>();
    if (omethod == "rprop")
      o.reset(new RPropOptimizer(num_feats));  // TODO add configuration
    else
      o.reset(new LBFGSOptimizer(num_feats, conf["correction_buffers"].as<int>()));
    cerr << "Optimizer: " << o->Name() << endl;
  }
  double objective = 0;
  vector<double> lambdas(num_feats, 0.0);
  weights.InitVector(&lambdas);
  if (lambdas.size() != num_feats) {
    cerr << "Initial weights file did not have all features specified!\n  feats="
         << num_feats << "\n  weights file=" << lambdas.size() << endl;
    lambdas.resize(num_feats, 0.0);
  }
  vector<double> gradient(num_feats, 0.0);
  vector<double> rcv_grad(num_feats, 0.0);
  bool converged = false;

  vector<string> corpus;
  if (shard_dir.size()) {
    ostringstream os; os << shard_dir << "/corpus." << rank << "_of_" << size;
    ReadTrainingCorpus(os.str(), 0, 1, &corpus);
    cerr << os.str() << " has " << corpus.size() << " training examples. " << endl;
    if (corpus.size() > 500) { corpus.resize(500); cerr << "  TRUNCATING\n"; }
  } else {
    ReadTrainingCorpus(conf["training_data"].as<string>(), rank, size, &corpus);
  }
  assert(corpus.size() > 0);

  TrainingObserver observer;
  while (!converged) {
    observer.Reset();
    if (rank == 0) {
      cerr << "Starting decoding... (~" << corpus.size() << " sentences / proc)\n";
    }
    decoder->SetWeights(lambdas);
    for (int i = 0; i < corpus.size(); ++i)
      decoder->Decode(corpus[i], &observer);

    fill(gradient.begin(), gradient.end(), 0);
    fill(rcv_grad.begin(), rcv_grad.end(), 0);
    observer.SetLocalGradientAndObjective(&gradient, &objective);

    double to = 0;
#ifdef HAVE_MPI
    MPI::COMM_WORLD.Reduce(const_cast<double*>(&gradient.data()[0]), &rcv_grad[0], num_feats, MPI::DOUBLE, MPI::SUM, 0);
    MPI::COMM_WORLD.Reduce(&objective, &to, 1, MPI::DOUBLE, MPI::SUM, 0);
    swap(gradient, rcv_grad);
    objective = to;
#endif

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
#ifdef HAVE_MPI
    MPI::COMM_WORLD.Bcast(const_cast<double*>(&lambdas.data()[0]), num_feats, MPI::DOUBLE, 0);
    MPI::COMM_WORLD.Bcast(&cint, 1, MPI::INT, 0);
    MPI::COMM_WORLD.Barrier();
#endif
    converged = cint;
  }
#ifdef HAVE_MPI
  MPI::Finalize(); 
#endif
  return 0;
}
