#include <sstream>
#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>
#include <tr1/memory>

#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "stringlib.h"
#include "hg_sampler.h"
#include "sentence_metadata.h"
#include "ns.h"
#include "ns_docscorer.h"
#include "verbose.h"
#include "viterbi.h"
#include "hg.h"
#include "prob.h"
#include "kbest.h"
#include "ff_register.h"
#include "decoder.h"
#include "filelib.h"
#include "fdict.h"
#include "weights.h"
#include "sparse_vector.h"
#include "sampler.h"

using namespace std;
namespace po = boost::program_options;

bool invert_score;
std::tr1::shared_ptr<MT19937> rng;

void RandomPermutation(int len, vector<int>* p_ids) {
  vector<int>& ids = *p_ids;
  ids.resize(len);
  for (int i = 0; i < len; ++i) ids[i] = i;
  for (int i = len; i > 0; --i) {
    int j = rng->next() * i;
    if (j == i) i--;
    swap(ids[i-1], ids[j]);
  }  
}

bool InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("input_weights,w",po::value<string>(),"Input feature weights file")
        ("source,i",po::value<string>(),"Source file for development set")
        ("passes,p", po::value<int>()->default_value(15), "Number of passes through the training data")
        ("reference,r",po::value<vector<string> >(), "[REQD] Reference translation(s) (tokenized text file)")
        ("mt_metric,m",po::value<string>()->default_value("ibm_bleu"), "Scoring metric (ibm_bleu, nist_bleu, koehn_bleu, ter, combi)")
        ("max_step_size,C", po::value<double>()->default_value(0.01), "regularization strength (C)")
        ("mt_metric_scale,s", po::value<double>()->default_value(1.0), "Amount to scale MT loss function by")
        ("k_best_size,k", po::value<int>()->default_value(250), "Size of hypothesis list to search for oracles")
        ("sample_forest,f", "Instead of a k-best list, sample k hypotheses from the decoder's forest")
        ("sample_forest_unit_weight_vector,x", "Before sampling (must use -f option), rescale the weight vector used so it has unit length; this may improve the quality of the samples")
        ("random_seed,S", po::value<uint32_t>(), "Random seed (if not specified, /dev/random will be used)")
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

  if (conf->count("help") || !conf->count("input_weights") || !conf->count("source") || !conf->count("decoder_config") || !conf->count("reference")) {
    cerr << dcmdline_options << endl;
    return false;
  }
  return true;
}

static const double kMINUS_EPSILON = -1e-6;

struct HypothesisInfo {
  SparseVector<double> features;
  double mt_metric;
};

struct GoodBadOracle {
  std::tr1::shared_ptr<HypothesisInfo> good;
  std::tr1::shared_ptr<HypothesisInfo> bad;
};

struct TrainingObserver : public DecoderObserver {
  TrainingObserver(const int k, const DocumentScorer& d, const EvaluationMetric& m, bool sf, vector<GoodBadOracle>* o) : ds(d), metric(m), oracles(*o), kbest_size(k), sample_forest(sf) {}
  const DocumentScorer& ds;
  const EvaluationMetric& metric;
  vector<GoodBadOracle>& oracles;
  std::tr1::shared_ptr<HypothesisInfo> cur_best;
  const int kbest_size;
  const bool sample_forest;

  const HypothesisInfo& GetCurrentBestHypothesis() const {
    return *cur_best;
  }

  virtual void NotifyTranslationForest(const SentenceMetadata& smeta, Hypergraph* hg) {
    UpdateOracles(smeta.GetSentenceID(), *hg);
  }

  std::tr1::shared_ptr<HypothesisInfo> MakeHypothesisInfo(const SparseVector<double>& feats, const double score) {
    std::tr1::shared_ptr<HypothesisInfo> h(new HypothesisInfo);
    h->features = feats;
    h->mt_metric = score;
    return h;
  }

  void UpdateOracles(int sent_id, const Hypergraph& forest) {
    std::tr1::shared_ptr<HypothesisInfo>& cur_good = oracles[sent_id].good;
    std::tr1::shared_ptr<HypothesisInfo>& cur_bad = oracles[sent_id].bad;
    cur_bad.reset();  // TODO get rid of??

    if (sample_forest) {
      vector<WordID> cur_prediction;
      ViterbiESentence(forest, &cur_prediction);
      SufficientStats sstats;
      ds[sent_id]->Evaluate(cur_prediction, &sstats);
      float sentscore = metric.ComputeScore(sstats);
      cur_best = MakeHypothesisInfo(ViterbiFeatures(forest), sentscore);

      vector<HypergraphSampler::Hypothesis> samples;
      HypergraphSampler::sample_hypotheses(forest, kbest_size, &*rng, &samples);
      for (unsigned i = 0; i < samples.size(); ++i) {
        ds[sent_id]->Evaluate(samples[i].words, &sstats);
        float sentscore = metric.ComputeScore(sstats);
        if (invert_score) sentscore *= -1.0;
        if (!cur_good || sentscore > cur_good->mt_metric)
          cur_good = MakeHypothesisInfo(samples[i].fmap, sentscore);
        if (!cur_bad || sentscore < cur_bad->mt_metric)
          cur_bad = MakeHypothesisInfo(samples[i].fmap, sentscore);
      }
    } else {
      KBest::KBestDerivations<vector<WordID>, ESentenceTraversal> kbest(forest, kbest_size);
      SufficientStats sstats;
      for (int i = 0; i < kbest_size; ++i) {
        const KBest::KBestDerivations<vector<WordID>, ESentenceTraversal>::Derivation* d =
          kbest.LazyKthBest(forest.nodes_.size() - 1, i);
        if (!d) break;
        ds[sent_id]->Evaluate(d->yield, &sstats);
        float sentscore = metric.ComputeScore(sstats);
        if (invert_score) sentscore *= -1.0;
        // cerr << TD::GetString(d->yield) << " ||| " << d->score << " ||| " << sentscore << endl;
        if (i == 0)
          cur_best = MakeHypothesisInfo(d->feature_values, sentscore);
        if (!cur_good || sentscore > cur_good->mt_metric)
          cur_good = MakeHypothesisInfo(d->feature_values, sentscore);
        if (!cur_bad || sentscore < cur_bad->mt_metric)
          cur_bad = MakeHypothesisInfo(d->feature_values, sentscore);
      }
      //cerr << "GOOD: " << cur_good->mt_metric << endl;
      //cerr << " CUR: " << cur_best->mt_metric << endl;
      //cerr << " BAD: " << cur_bad->mt_metric << endl;
    }
  }
};

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

bool ApproxEqual(double a, double b) {
  if (a == b) return true;
  return (fabs(a-b)/fabs(b)) < 0.000001;
}

int main(int argc, char** argv) {
  register_feature_functions();
  SetSilent(true);  // turn off verbose decoder output

  po::variables_map conf;
  if (!InitCommandLine(argc, argv, &conf)) return 1;

  if (conf.count("random_seed"))
    rng.reset(new MT19937(conf["random_seed"].as<uint32_t>()));
  else
    rng.reset(new MT19937);
  const bool sample_forest = conf.count("sample_forest") > 0;
  const bool sample_forest_unit_weight_vector = conf.count("sample_forest_unit_weight_vector") > 0;
  if (sample_forest_unit_weight_vector && !sample_forest) {
    cerr << "Cannot --sample_forest_unit_weight_vector without --sample_forest" << endl;
    return 1;
  }
  vector<string> corpus;
  ReadTrainingCorpus(conf["source"].as<string>(), &corpus);

  string metric_name = UppercaseString(conf["mt_metric"].as<string>());
  if (metric_name == "COMBI") {
    cerr << "WARNING: 'combi' metric is no longer supported, switching to 'COMB:TER=-0.5;IBM_BLEU=0.5'\n";
    metric_name = "COMB:TER=-0.5;IBM_BLEU=0.5";
  } else if (metric_name == "BLEU") {
    cerr << "WARNING: 'BLEU' is ambiguous, assuming 'IBM_BLEU'\n";
    metric_name = "IBM_BLEU";
  }
  EvaluationMetric* metric = EvaluationMetric::Instance(metric_name);
  DocumentScorer ds(metric, conf["reference"].as<vector<string> >());
  cerr << "Loaded " << ds.size() << " references for scoring with " << metric_name << endl;
  invert_score = metric->IsErrorMetric();

  if (ds.size() != corpus.size()) {
    cerr << "Mismatched number of references (" << ds.size() << ") and sources (" << corpus.size() << ")\n";
    return 1;
  }

  ReadFile ini_rf(conf["decoder_config"].as<string>());
  Decoder decoder(ini_rf.stream());

  // load initial weights
  vector<weight_t>& dense_weights = decoder.CurrentWeightVector();
  SparseVector<weight_t> lambdas;
  Weights::InitFromFile(conf["input_weights"].as<string>(), &dense_weights);
  Weights::InitSparseVector(dense_weights, &lambdas);

  const double max_step_size = conf["max_step_size"].as<double>();
  const double mt_metric_scale = conf["mt_metric_scale"].as<double>();

  assert(corpus.size() > 0);
  vector<GoodBadOracle> oracles(corpus.size());

  TrainingObserver observer(conf["k_best_size"].as<int>(), ds, *metric, sample_forest, &oracles);
  int cur_sent = 0;
  int lcount = 0;
  int normalizer = 0;
  double tot_loss = 0;
  int dots = 0;
  int cur_pass = 0;
  SparseVector<double> tot;
  tot += lambdas;          // initial weights
  normalizer++;            // count for initial weights
  int max_iteration = conf["passes"].as<int>() * corpus.size();
  string msg = "# MIRA tuned weights";
  string msga = "# MIRA tuned weights AVERAGED";
  vector<int> order;
  RandomPermutation(corpus.size(), &order);
  while (lcount <= max_iteration) {
    lambdas.init_vector(&dense_weights);
    if ((cur_sent * 40 / corpus.size()) > dots) { ++dots; cerr << '.'; }
    if (corpus.size() == cur_sent) {
      cerr << " [AVG METRIC LAST PASS=" << (tot_loss / corpus.size()) << "]\n";
      Weights::ShowLargestFeatures(dense_weights);
      cur_sent = 0;
      tot_loss = 0;
      dots = 0;
      ostringstream os;
      os << "weights.mira-pass" << (cur_pass < 10 ? "0" : "") << cur_pass << ".gz";
      SparseVector<double> x = tot;
      x /= normalizer;
      ostringstream sa;
      sa << "weights.mira-pass" << (cur_pass < 10 ? "0" : "") << cur_pass << "-avg.gz";
      x.init_vector(&dense_weights);
      Weights::WriteToFile(os.str(), dense_weights, true, &msg);
      ++cur_pass;
      RandomPermutation(corpus.size(), &order);
    }
    if (cur_sent == 0) {
      cerr << "PASS " << (lcount / corpus.size() + 1) << endl;
    }
    decoder.SetId(order[cur_sent]);
    double sc = 1.0;
    if (sample_forest_unit_weight_vector) {
      sc = lambdas.l2norm();
      if (sc > 0) {
        for (unsigned i = 0; i < dense_weights.size(); ++i)
          dense_weights[i] /= sc;
      }
    }
    decoder.Decode(corpus[order[cur_sent]], &observer);  // update oracles
    if (sc && sc != 1.0) {
      for (unsigned i = 0; i < dense_weights.size(); ++i)
        dense_weights[i] *= sc;
    }
    const HypothesisInfo& cur_hyp = observer.GetCurrentBestHypothesis();
    const HypothesisInfo& cur_good = *oracles[order[cur_sent]].good;
    const HypothesisInfo& cur_bad = *oracles[order[cur_sent]].bad;
    tot_loss += cur_hyp.mt_metric;
    if (!ApproxEqual(cur_hyp.mt_metric, cur_good.mt_metric)) {
      const double loss = cur_bad.features.dot(dense_weights) - cur_good.features.dot(dense_weights) +
          mt_metric_scale * (cur_good.mt_metric - cur_bad.mt_metric);
      //cerr << "LOSS: " << loss << endl;
      if (loss > 0.0) {
        SparseVector<double> diff = cur_good.features;
        diff -= cur_bad.features;
        double step_size = loss / diff.l2norm_sq();
        //cerr << loss << " " << step_size << " " << diff << endl;
        if (step_size > max_step_size) step_size = max_step_size;
        lambdas += (cur_good.features * step_size);
        lambdas -= (cur_bad.features * step_size);
        //cerr << "L: " << lambdas << endl;
      }
    }
    tot += lambdas;
    ++normalizer;
    ++lcount;
    ++cur_sent;
  }
  cerr << endl;
  Weights::WriteToFile("weights.mira-final.gz", dense_weights, true, &msg);
  tot /= normalizer;
  tot.init_vector(dense_weights);
  msg = "# MIRA tuned weights (averaged vector)";
  Weights::WriteToFile("weights.mira-final-avg.gz", dense_weights, true, &msg);
  cerr << "Optimization complete.\nAVERAGED WEIGHTS: weights.mira-final-avg.gz\n";
  return 0;
}

