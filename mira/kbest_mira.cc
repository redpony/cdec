#include <sstream>
#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>

#include "config.h"

#include <boost/shared_ptr.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "sentence_metadata.h"
#include "scorer.h"
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

using namespace std;
using boost::shared_ptr;
namespace po = boost::program_options;

bool invert_score;

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
  shared_ptr<HypothesisInfo> good;
  shared_ptr<HypothesisInfo> bad;
};

struct TrainingObserver : public DecoderObserver {
  TrainingObserver(const int k, const DocScorer& d, vector<GoodBadOracle>* o) : ds(d), oracles(*o), kbest_size(k) {}
  const DocScorer& ds;
  vector<GoodBadOracle>& oracles;
  shared_ptr<HypothesisInfo> cur_best;
  const int kbest_size;

  const HypothesisInfo& GetCurrentBestHypothesis() const {
    return *cur_best;
  }

  virtual void NotifyTranslationForest(const SentenceMetadata& smeta, Hypergraph* hg) {
    UpdateOracles(smeta.GetSentenceID(), *hg);
  }

  shared_ptr<HypothesisInfo> MakeHypothesisInfo(const SparseVector<double>& feats, const double score) {
    shared_ptr<HypothesisInfo> h(new HypothesisInfo);
    h->features = feats;
    h->mt_metric = score;
    return h;
  }

  void UpdateOracles(int sent_id, const Hypergraph& forest) {
    shared_ptr<HypothesisInfo>& cur_good = oracles[sent_id].good;
    shared_ptr<HypothesisInfo>& cur_bad = oracles[sent_id].bad;
    cur_bad.reset();  // TODO get rid of??
    KBest::KBestDerivations<vector<WordID>, ESentenceTraversal> kbest(forest, kbest_size);
    for (int i = 0; i < kbest_size; ++i) {
      const KBest::KBestDerivations<vector<WordID>, ESentenceTraversal>::Derivation* d =
        kbest.LazyKthBest(forest.nodes_.size() - 1, i);
      if (!d) break;
      float sentscore = ds[sent_id]->ScoreCandidate(d->yield)->ComputeScore();
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

  vector<string> corpus;
  ReadTrainingCorpus(conf["source"].as<string>(), &corpus);
  const string metric_name = conf["mt_metric"].as<string>();
  ScoreType type = ScoreTypeFromString(metric_name);
  if (type == TER) {
    invert_score = true;
  } else {
    invert_score = false;
  }
  DocScorer ds(type, conf["reference"].as<vector<string> >(), "");
  cerr << "Loaded " << ds.size() << " references for scoring with " << metric_name << endl;
  if (ds.size() != corpus.size()) {
    cerr << "Mismatched number of references (" << ds.size() << ") and sources (" << corpus.size() << ")\n";
    return 1;
  }
  // load initial weights
  Weights weights;
  weights.InitFromFile(conf["input_weights"].as<string>());
  SparseVector<double> lambdas;
  weights.InitSparseVector(&lambdas);

  ReadFile ini_rf(conf["decoder_config"].as<string>());
  Decoder decoder(ini_rf.stream());
  const double max_step_size = conf["max_step_size"].as<double>();
  const double mt_metric_scale = conf["mt_metric_scale"].as<double>();

  assert(corpus.size() > 0);
  vector<GoodBadOracle> oracles(corpus.size());

  TrainingObserver observer(conf["k_best_size"].as<int>(), ds, &oracles);
  int cur_sent = 0;
  int lcount = 0;
  double tot_loss = 0;
  int dots = 0;
  int cur_pass = 0;
  vector<double> dense_weights;
  SparseVector<double> tot;
  tot += lambdas;          // initial weights
  lcount++;                // count for initial weights
  int max_iteration = conf["passes"].as<int>() * corpus.size();
  string msg = "# MIRA tuned weights";
  while (lcount <= max_iteration) {
    dense_weights.clear();
    weights.InitFromVector(lambdas);
    weights.InitVector(&dense_weights);
    decoder.SetWeights(dense_weights);
    if ((cur_sent * 40 / corpus.size()) > dots) { ++dots; cerr << '.'; }
    if (corpus.size() == cur_sent) {
      cur_sent = 0;
      cerr << " [AVG METRIC LAST PASS=" << (tot_loss / corpus.size()) << "]\n";
      tot_loss = 0;
      dots = 0;
      ostringstream os;
      os << "weights.mira-pass" << (cur_pass < 10 ? "0" : "") << cur_pass << ".gz";
      weights.WriteToFile(os.str(), true, &msg);
      ++cur_pass;
    }
    if (cur_sent == 0) { cerr << "PASS " << (lcount / corpus.size() + 1) << endl << lambdas << endl; }
    decoder.SetId(cur_sent);
    decoder.Decode(corpus[cur_sent], &observer);  // update oracles
    const HypothesisInfo& cur_hyp = observer.GetCurrentBestHypothesis();
    const HypothesisInfo& cur_good = *oracles[cur_sent].good;
    const HypothesisInfo& cur_bad = *oracles[cur_sent].bad;
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
    ++lcount;
    ++cur_sent;
  }
  cerr << endl;
  weights.WriteToFile("weights.mira-final.gz", true, &msg);
  tot /= lcount;
  weights.InitFromVector(tot);
  msg = "# MIRA tuned weights (averaged vector)";
  weights.WriteToFile("weights.mira-final-avg.gz", true, &msg);
  cerr << "Optimization complete.\\AVERAGED WEIGHTS: weights.mira-final-avg.gz\n";
  return 0;
}

