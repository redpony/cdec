#include <sstream>
#include <iostream>
#include <fstream>
#include <vector>
#include <cassert>
#include <cmath>

#include <boost/shared_ptr.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "stringlib.h"
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
        ("cdec_config,c",po::value<string>(),"Decoder configuration file")
        ("weights,w",po::value<string>(),"Initial feature weights")
        ("training_data,d",po::value<string>(),"Training data")
        ("minibatch_size_per_proc,s", po::value<unsigned>()->default_value(6), "Number of training instances evaluated per processor in each minibatch")
        ("minibatch_iterations,i", po::value<unsigned>()->default_value(10), "Number of optimization iterations per minibatch")
        ("iterations,I", po::value<unsigned>()->default_value(50), "Number of passes through the training data before termination")
        ("regularization_strength,C", po::value<double>()->default_value(0.2), "Regularization strength")
        ("time_series_strength,T", po::value<double>()->default_value(0.0), "Time series regularization strength")
        ("random_seed,S", po::value<uint32_t>(), "Random seed (if not specified, /dev/random will be used)")
        ("lbfgs_memory_buffers,M", po::value<unsigned>()->default_value(10), "Number of memory buffers for LBFGS history");
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

  if (conf->count("help") || !conf->count("training_data") || !conf->count("cdec_config")) {
    cerr << "LBFGS minibatch online optimizer (MPI support "
#if HAVE_MPI
         << "enabled"
#else
         << "not enabled"
#endif
         << ")\n" << dcmdline_options << endl;
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

struct CopyHGsObserver : public DecoderObserver {
  Hypergraph* hg_;
  Hypergraph* gold_hg_;

  // this can free up some memory
  void RemoveRules(Hypergraph* h) {
    for (unsigned i = 0; i < h->edges_.size(); ++i)
      h->edges_[i].rule_.reset();
  }

  void SetCurrentHypergraphs(Hypergraph* h, Hypergraph* gold_h) {
    hg_ = h;
    gold_hg_ = gold_h;
  }

  virtual void NotifyDecodingStart(const SentenceMetadata&) {
    state = 1;
  }

  // compute model expectations, denominator of objective
  virtual void NotifyTranslationForest(const SentenceMetadata&, Hypergraph* hg) {
    *hg_ = *hg;
    RemoveRules(hg_);
    assert(state == 1);
    state = 2;
  }

  // compute "empirical" expectations, numerator of objective
  virtual void NotifyAlignmentForest(const SentenceMetadata&, Hypergraph* hg) {
    assert(state == 2);
    state = 3;
    *gold_hg_ = *hg;
    RemoveRules(gold_hg_);
  }

  virtual void NotifyDecodingComplete(const SentenceMetadata&) {
    if (state == 3) {
    } else {
      hg_->clear();
      gold_hg_->clear();
    }
  }

  int state;
};

void ReadConfig(const string& ini, istringstream* out) {
  ReadFile rf(ini);
  istream& in = *rf.stream();
  ostringstream os;
  while(in) {
    string line;
    getline(in, line);
    if (!in) continue;
    os << line << endl;
  }
  out->str(os.str());
}

#ifdef HAVE_MPI
namespace boost { namespace mpi {
  template<>
  struct is_commutative<std::plus<SparseVector<double> >, SparseVector<double> > 
    : mpl::true_ { };
} } // end namespace boost::mpi
#endif

void AddGrad(const SparseVector<prob_t> x, double s, SparseVector<double>* acc) {
  for (SparseVector<prob_t>::const_iterator it = x.begin(); it != x.end(); ++it)
    acc->add_value(it->first, it->second.as_float() * s);
}

double PNorm(const vector<double>& v, const double p) {
  double acc = 0;
  for (int i = 0; i < v.size(); ++i)
    acc += pow(v[i], p);
  return pow(acc, 1.0 / p);
}

void VV(ostream&os, const vector<double>& v) {
  for (int i = 1; i < v.size(); ++i)
    if (v[i]) os << FD::Convert(i) << "=" << v[i] << " ";
}

double ApplyRegularizationTerms(const double C,
                                const double T,
                                const vector<double>& weights,
                                const vector<double>& prev_weights,
                                double* g) {
  double reg = 0;
  for (size_t i = 0; i < weights.size(); ++i) {
    const double prev_w_i = (i < prev_weights.size() ? prev_weights[i] : 0.0);
    const double& w_i = weights[i];
    reg += C * w_i * w_i;
    g[i] += 2 * C * w_i;

    reg += T * (w_i - prev_w_i) * (w_i - prev_w_i);
    g[i] += 2 * T * (w_i - prev_w_i);
  }
  return reg;
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
  MT19937* rng = NULL;

  po::variables_map conf;
  if (!InitCommandLine(argc, argv, &conf))
    return 1;

  boost::shared_ptr<BatchOptimizer> o;
  const unsigned lbfgs_memory_buffers = conf["lbfgs_memory_buffers"].as<unsigned>();
  const unsigned size_per_proc = conf["minibatch_size_per_proc"].as<unsigned>();
  const unsigned minibatch_iterations = conf["minibatch_iterations"].as<unsigned>();
  const double regularization_strength = conf["regularization_strength"].as<double>();
  const double time_series_strength = conf["time_series_strength"].as<double>();
  const bool use_time_series_reg = time_series_strength > 0.0;
  const unsigned max_iteration = conf["iterations"].as<unsigned>();

  vector<string> corpus;
  vector<int> ids;
  ReadTrainingCorpus(conf["training_data"].as<string>(), rank, size, &corpus, &ids);
  assert(corpus.size() > 0);

  if (size_per_proc > corpus.size()) {
    cerr << "Minibatch size (per processor) must be smaller or equal to the local corpus size!\n";
    return 1;
  }

  // initialize decoder (loads hash functions if necessary)
  istringstream ins;
  ReadConfig(conf["cdec_config"].as<string>(), &ins);
  Decoder decoder(&ins);

  // load initial weights
  vector<weight_t> prev_weights;
  if (conf.count("weights"))
    Weights::InitFromFile(conf["weights"].as<string>(), &prev_weights);

  if (conf.count("random_seed"))
    rng = new MT19937(conf["random_seed"].as<uint32_t>());
  else
    rng = new MT19937;

  size_t total_corpus_size = 0;
#ifdef HAVE_MPI
  reduce(world, corpus.size(), total_corpus_size, std::plus<size_t>(), 0);
#else
  total_corpus_size = corpus.size();
#endif

  if (rank == 0)
    cerr << "Total corpus size: " << total_corpus_size << endl;

  CopyHGsObserver observer;

  int write_weights_every_ith = 100; // TODO configure
  int titer = -1;

  vector<weight_t>& cur_weights = decoder.CurrentWeightVector();
  if (use_time_series_reg) {
    cur_weights = prev_weights;
  } else {
    cur_weights.swap(prev_weights);
    prev_weights.clear();
  }

  int iter = -1;
  bool converged = false;
  vector<double> gg;
  while (!converged) {
#ifdef HAVE_MPI
    mpi::timer timer;
#endif
    ++iter; ++titer;
    if (rank == 0) {
      converged = (iter == max_iteration);
        string fname = "weights.cur.gz";
        if (iter % write_weights_every_ith == 0) {
          ostringstream o; o << "weights.epoch_" << iter << ".gz";
          fname = o.str();
        }
        if (converged) { fname = "weights.final.gz"; }
        ostringstream vv;
        vv << "total iter=" << titer << " (of current config iter=" << iter << ")  minibatch=" << size_per_proc << " sentences/proc x " << size << " procs.   num_feats=" << FD::NumFeats() << "   passes_thru_data=" << (titer * size_per_proc / static_cast<double>(corpus.size()));
        const string svv = vv.str();
        Weights::WriteToFile(fname, cur_weights, true, &svv);
      }

      vector<Hypergraph> hgs(size_per_proc);
      vector<Hypergraph> gold_hgs(size_per_proc);
      for (int i = 0; i < size_per_proc; ++i) {
        int ei = corpus.size() * rng->next();
        int id = ids[ei];
        observer.SetCurrentHypergraphs(&hgs[i], &gold_hgs[i]);
        decoder.SetId(id);
        decoder.Decode(corpus[ei], &observer);
      }

      SparseVector<double> local_grad, g;
      double local_obj = 0;
      o.reset();
      for (unsigned mi = 0; mi < minibatch_iterations; ++mi) {
        local_grad.clear();
        g.clear();
        local_obj = 0;

        for (unsigned i = 0; i < size_per_proc; ++i) {
          Hypergraph& hg = hgs[i];
          Hypergraph& hg_gold = gold_hgs[i];
          if (hg.edges_.size() < 2) continue;

          hg.Reweight(cur_weights);
          hg_gold.Reweight(cur_weights);
          SparseVector<prob_t> model_exp, gold_exp;
          const prob_t z = InsideOutside<prob_t,
                                         EdgeProb,
                                         SparseVector<prob_t>,
                                         EdgeFeaturesAndProbWeightFunction>(hg, &model_exp);
          local_obj += log(z);
          model_exp /= z;
          AddGrad(model_exp, 1.0, &local_grad);
          model_exp.clear();

          const prob_t goldz = InsideOutside<prob_t,
                                         EdgeProb,
                                         SparseVector<prob_t>,
                                         EdgeFeaturesAndProbWeightFunction>(hg_gold, &gold_exp);
          local_obj -= log(goldz);

          if (log(z) - log(goldz) < kMINUS_EPSILON) {
            cerr << "DIFF. ERR! log_model_z < log_gold_z: " << log(z) << " " << log(goldz) << endl;
            return 1;
          }

          gold_exp /= goldz;
          AddGrad(gold_exp, -1.0, &local_grad);
        }

        double obj = 0;
#ifdef HAVE_MPI
        reduce(world, local_obj, obj, std::plus<double>(), 0);
        reduce(world, local_grad, g, std::plus<SparseVector<double> >(), 0);
#else
        obj = local_obj;
        g.swap(local_grad);
#endif
        local_grad.clear();
        if (rank == 0) {
          // g /= (size_per_proc * size);
          if (!o)
            o.reset(new LBFGSOptimizer(FD::NumFeats(), lbfgs_memory_buffers));
          gg.clear();
          gg.resize(FD::NumFeats());
          if (gg.size() != cur_weights.size()) { cur_weights.resize(gg.size()); }
          for (SparseVector<double>::iterator it = g.begin(); it != g.end(); ++it)
            if (it->first) { gg[it->first] = it->second; }
          g.clear();
          double r = ApplyRegularizationTerms(regularization_strength,
                                time_series_strength, // * (iter == 0 ? 0.0 : 1.0),
                                cur_weights,
                                prev_weights,
                                &gg[0]);
          obj += r;
          if (mi == 0 || mi == (minibatch_iterations - 1)) {
            if (!mi) cerr << iter << ' '; else cerr << ' ';
            cerr << "OBJ=" << obj << " (REG=" << r << ")" << " |g|=" << PNorm(gg, 2) << " |w|=" << PNorm(cur_weights, 2); 
            if (mi > 0) cerr << endl << flush; else cerr << ' ';
          } else { cerr << '.' << flush; }
          // cerr << "w = "; VV(cerr, cur_weights); cerr << endl;
          // cerr << "g = "; VV(cerr, gg); cerr << endl;
          o->Optimize(obj, gg, &cur_weights);
        }
#ifdef HAVE_MPI
        broadcast(world, cur_weights, 0);
        broadcast(world, converged, 0);
        world.barrier();
#endif
    }
    prev_weights = cur_weights;
  }
  return 0;
}
