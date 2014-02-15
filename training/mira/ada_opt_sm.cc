#include "config.h"

#include <boost/container/flat_map.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "filelib.h"
#include "stringlib.h"
#include "weights.h"
#include "sparse_vector.h"
#include "candidate_set.h"
#include "sentence_metadata.h"
#include "ns.h"
#include "ns_docscorer.h"
#include "verbose.h"
#include "hg.h"
#include "ff_register.h"
#include "decoder.h"
#include "fdict.h"
#include "sampler.h"

using namespace std;
namespace po = boost::program_options;

boost::shared_ptr<MT19937> rng;
vector<training::CandidateSet> kbests;
SparseVector<weight_t> G, u, lambdas;
double pseudo_doc_decay = 0.9;

bool InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
    ("decoder_config,c",po::value<string>(),"[REQ] Decoder configuration file")
    ("devset,d",po::value<string>(),"[REQ] Source/reference development set")
    ("weights,w",po::value<string>(),"Initial feature weights file")
    ("mt_metric,m",po::value<string>()->default_value("ibm_bleu"), "Scoring metric (ibm_bleu, nist_bleu, koehn_bleu, ter, combi)")
    ("size",po::value<unsigned>()->default_value(0), "Process rank (for multiprocess mode)")
    ("rank",po::value<unsigned>()->default_value(1), "Number of processes (for multiprocess mode)")
    ("optimizer,o",po::value<unsigned>()->default_value(1), "Optimizer (Adaptive MIRA=1)")
    ("fear,f",po::value<unsigned>()->default_value(1), "Fear selection (model-cost=1, maxcost=2, maxscore=3)")
    ("hope,h",po::value<unsigned>()->default_value(1), "Hope selection (model+cost=1, mincost=2)")
    ("eta0", po::value<double>()->default_value(0.1), "Initial step size")
    ("random_seed,S", po::value<uint32_t>(), "Random seed (if not specified, /dev/random will be used)")
    ("mt_metric_scale,s", po::value<double>()->default_value(1.0), "Scale MT loss function by this amount")
    ("pseudo_doc,e", "Use pseudo-documents for approximate scoring")
    ("k_best_size,k", po::value<unsigned>()->default_value(500), "Size of hypothesis list to search for oracles");
  po::options_description clo("Command line options");
  clo.add_options()
    ("config", po::value<string>(), "Configuration file")
    ("help,H", "Print this help message and exit");
  po::options_description dconfig_options, dcmdline_options;
  dconfig_options.add(opts);
  dcmdline_options.add(opts).add(clo);
  
  po::store(parse_command_line(argc, argv, dcmdline_options), *conf);
  if (conf->count("config")) {
    ifstream config((*conf)["config"].as<string>().c_str());
    po::store(po::parse_config_file(config, dconfig_options), *conf);
  }
  po::notify(*conf);

  if (conf->count("help")
          || !conf->count("decoder_config")
          || !conf->count("devset")) {
    cerr << dcmdline_options << endl;
    return false;
  }
  return true;
}

struct TrainingObserver : public DecoderObserver {
  explicit TrainingObserver(const EvaluationMetric& m, const int k) : metric(m), kbest_size(k), cur_eval() {}

  const EvaluationMetric& metric;
  const int kbest_size;
  const SegmentEvaluator* cur_eval;
  SufficientStats pdoc;
  unsigned hi, vi, fi;  // hope, viterbi, fear

  void SetSegmentEvaluator(const SegmentEvaluator* eval) {
    cur_eval = eval;
  }

  virtual void NotifySourceParseFailure(const SentenceMetadata& smeta) {
    cerr << "Failed to translate sentence with ID = " << smeta.GetSentenceID() << endl;
    abort();
  }

  unsigned CostAugmentedDecode(const training::CandidateSet& cs,
                               const SparseVector<double>& w,
                               double alpha = 0) {
    unsigned best_i = 0;
    double best = -numeric_limits<double>::infinity();
    for (unsigned i = 0; i < cs.size(); ++i) {
      double s = cs[i].fmap.dot(w);
      if (alpha)
        s += alpha * metric.ComputeScore(cs[i].eval_feats + pdoc);
      if (s > best) {
        best = s;
        best_i = i;
      }
    }
    return best_i;
  }

  virtual void NotifyTranslationForest(const SentenceMetadata& smeta, Hypergraph* hg) {
    pdoc *= pseudo_doc_decay;
    const unsigned sent_id = smeta.GetSentenceID();
    kbests[sent_id].AddUniqueKBestCandidates(*hg, kbest_size, cur_eval);
    vi = CostAugmentedDecode(kbests[sent_id], lambdas);
    hi = CostAugmentedDecode(kbests[sent_id], lambdas, 1.0);
    fi = CostAugmentedDecode(kbests[sent_id], lambdas, -1.0);
    cerr << sent_id << " ||| " << TD::GetString(kbests[sent_id][vi].ewords) << " ||| " << metric.ComputeScore(kbests[sent_id][vi].eval_feats + pdoc) << endl;
    pdoc += kbests[sent_id][vi].eval_feats;  // update pseudodoc stats
  }
};

int main(int argc, char** argv) {
  SetSilent(true);  // turn off verbose decoder output
  register_feature_functions();

  po::variables_map conf;
  if (!InitCommandLine(argc, argv, &conf)) return 1;

  if (conf.count("random_seed"))
    rng.reset(new MT19937(conf["random_seed"].as<uint32_t>()));
  else
    rng.reset(new MT19937);
  
  string metric_name = UppercaseString(conf["mt_metric"].as<string>());
  if (metric_name == "COMBI") {
    cerr << "WARNING: 'combi' metric is no longer supported, switching to 'COMB:TER=-0.5;IBM_BLEU=0.5'\n";
    metric_name = "COMB:TER=-0.5;IBM_BLEU=0.5";
  } else if (metric_name == "BLEU") {
    cerr << "WARNING: 'BLEU' is ambiguous, assuming 'IBM_BLEU'\n";
    metric_name = "IBM_BLEU";
  }
  EvaluationMetric* metric = EvaluationMetric::Instance(metric_name);
  DocumentScorer ds(metric, conf["devset"].as<string>());
  cerr << "Loaded " << ds.size() << " references for scoring with " << metric_name << endl;
  kbests.resize(ds.size());
  double eta = 0.001;

  ReadFile ini_rf(conf["decoder_config"].as<string>());
  Decoder decoder(ini_rf.stream());

  vector<weight_t>& dense_weights = decoder.CurrentWeightVector();
  if (conf.count("weights")) {
    Weights::InitFromFile(conf["weights"].as<string>(), &dense_weights);
    Weights::InitSparseVector(dense_weights, &lambdas);
  }

  TrainingObserver observer(*metric, conf["k_best_size"].as<unsigned>());

  unsigned num = 200;
  for (unsigned iter = 1; iter < num; ++iter) {
    lambdas.init_vector(&dense_weights);
    unsigned sent_id = rng->next() * ds.size();
    cerr << "Learning from sentence id: " << sent_id << endl;
    observer.SetSegmentEvaluator(ds[sent_id]);
    decoder.SetId(sent_id);
    decoder.Decode(ds[sent_id]->src, &observer);
    if (observer.vi != observer.hi) {  // viterbi != hope
      SparseVector<double> grad = kbests[sent_id][observer.fi].fmap;
      grad -= kbests[sent_id][observer.hi].fmap;
      cerr << "GRAD: " << grad << endl;
      const SparseVector<double>& g = grad;
#if HAVE_CXX11 && (__GNUC_MINOR__ > 4 || __GNUC__ > 4)
      for (auto& gi : g) {
#else
      for (SparseVector<double>::const_iterator it = g.begin(); it != g.end(); ++it) {
        const pair<unsigned,double>& gi = *it;
#endif
        if (gi.second) {
          u[gi.first] += gi.second;
          G[gi.first] += gi.second * gi.second;
          lambdas.set_value(gi.first, 1.0);  // this is a dummy value to trigger recomputation
        }
      }
      for (SparseVector<double>::iterator it = lambdas.begin(); it != lambdas.end(); ++it) {
        const pair<unsigned,double>& xi = *it;
        double z = fabs(u[xi.first] / iter) - 0.0;
        double s = 1;
        if (u[xi.first] > 0) s = -1;
        if (z > 0 && G[xi.first]) {
          lambdas.set_value(xi.first, eta * s * z * iter / sqrt(G[xi.first]));
        } else {
          lambdas.set_value(xi.first, 0.0);
        }
      }
    }
  }
  cerr << "Optimization complete.\n";
  Weights::WriteToFile("-", dense_weights, true);
  return 0;
}

