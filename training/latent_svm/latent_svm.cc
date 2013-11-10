/*
Points to note regarding variable names:
total_loss and prev_loss actually refer not to loss, but the metric (usually BLEU)
*/
#include <sstream>
#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>

//boost libraries
#include <boost/shared_ptr.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

//cdec libraries
#include "config.h"
#include "hg_sampler.h"
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
#include "sampler.h"

using namespace std;
namespace po = boost::program_options;

bool invert_score; 
boost::shared_ptr<MT19937> rng; //random seed ptr

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
        ("weights,w",po::value<string>(),"[REQD] Input feature weights file")
        ("input,i",po::value<string>(),"[REQD] Input source file for development set")
        ("passes,p", po::value<int>()->default_value(15), "Number of passes through the training data")
        ("weights_write_interval,n", po::value<int>()->default_value(1000), "Number of lines between writing out weights")
        ("reference,r",po::value<vector<string> >(), "[REQD] Reference translation(s) (tokenized text file)")
        ("mt_metric,m",po::value<string>()->default_value("ibm_bleu"), "Scoring metric (ibm_bleu, nist_bleu, koehn_bleu, ter, combi)")
        ("regularizer_strength,C", po::value<double>()->default_value(0.01), "regularization strength")
        ("mt_metric_scale,s", po::value<double>()->default_value(1.0), "Cost function is -mt_metric_scale*BLEU")
        ("costaug_log_bleu,l", "Flag converts BLEU to log space. Cost function is thus -mt_metric_scale*log(BLEU). Not on by default")
        ("average,A", "Average the weights (this is a weighted average due to the scaling factor)")
        ("mu,u", po::value<double>()->default_value(0.0), "weight (between 0 and 1) to scale model score by for oracle selection")
        ("stepsize_param,a", po::value<double>()->default_value(0.01), "Stepsize parameter, during optimization")
        ("stepsize_reduce,t", "Divide step size by sqrt(number of examples seen so far), as per Ratliff et al., 2007")
	("metric_threshold,T", po::value<double>()->default_value(0.0), "Threshold for diff between oracle BLEU and cost-aug BLEU for updating the weights")
	("check_positive,P", "Check that the loss is positive before updating")
        ("k_best_size,k", po::value<int>()->default_value(250), "Size of hypothesis list to search for oracles")
        ("best_ever,b", "Keep track of the best hypothesis we've ever seen (metric score), and use that as the reference")
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

  if (conf->count("help") || !conf->count("weights") || !conf->count("input") || !conf->count("decoder_config") || !conf->count("reference")) {
    cerr << dcmdline_options << endl;
    return false;
  }
  return true;
}

double scaling_trick = 1; // see http://blog.smola.org/post/940672544/fast-quadratic-regularization-for-online-learning
/*computes and returns cost augmented score for negative example selection*/
double cost_augmented_score(const LogVal<double> model_score, const double mt_metric_score, const double mt_metric_scale, const bool logbleu) {
  if(logbleu) {
    if(mt_metric_score != 0)
      // NOTE: log(model_score) is just the model score feature weights * features
      return log(model_score) * scaling_trick + (- mt_metric_scale * log(mt_metric_score));
    else
      return -1000000;
  }
  // NOTE: log(model_score) is just the model score feature weights * features
  return log(model_score) * scaling_trick + (- mt_metric_scale * mt_metric_score);
}

/*computes and returns mu score, for oracle selection*/
double muscore(const vector<weight_t>& feature_weights, const SparseVector<double>& feature_values, const double mt_metric_score, const double mu, const bool logbleu) {
  if(logbleu) {
    if(mt_metric_score != 0)
      return feature_values.dot(feature_weights) * mu + (1 - mu) * log(mt_metric_score);
    else
      return feature_values.dot(feature_weights) * mu + (1 - mu) * (-1000000);  // log(0) is -inf
  }
  return feature_values.dot(feature_weights) * mu + (1 - mu) * mt_metric_score;
}

static const double kMINUS_EPSILON = -1e-6;

struct HypothesisInfo {
  SparseVector<double> features;
  double mt_metric_score;
  // The model score changes when the feature weights change, so it is not stored here
  // It must be recomputed every time
};

struct GoodOracle {
  boost::shared_ptr<HypothesisInfo> good;
};

struct TrainingObserver : public DecoderObserver {
  TrainingObserver(const int k,
                   const DocScorer& d,
                   vector<GoodOracle>* o,
                   const vector<weight_t>& feat_weights,
                   const double metric_scale,
                   const double Mu,
                   const bool bestever,
                   const bool LogBleu) : ds(d), feature_weights(feat_weights), oracles(*o), kbest_size(k), mt_metric_scale(metric_scale), mu(Mu), best_ever(bestever), log_bleu(LogBleu) {}
  const DocScorer& ds;
  const vector<weight_t>& feature_weights;
  vector<GoodOracle>& oracles;
  boost::shared_ptr<HypothesisInfo> cur_best;
  boost::shared_ptr<HypothesisInfo> cur_costaug_best;
  boost::shared_ptr<HypothesisInfo> cur_ref; 
  const int kbest_size;
  const double mt_metric_scale;
  const double mu;
  const bool best_ever;
  const bool log_bleu;

  const HypothesisInfo& GetCurrentBestHypothesis() const {
    return *cur_best;
  }

  const HypothesisInfo& GetCurrentCostAugmentedHypothesis() const {
    return *cur_costaug_best;
  }

  const HypothesisInfo& GetCurrentReference() const {
    return *cur_ref; 
  }

  virtual void NotifyTranslationForest(const SentenceMetadata& smeta, Hypergraph* hg) {
    UpdateOracles(smeta.GetSentenceID(), *hg);
  }

  boost::shared_ptr<HypothesisInfo> MakeHypothesisInfo(const SparseVector<double>& feats, const double metric) {
    boost::shared_ptr<HypothesisInfo> h(new HypothesisInfo);
    h->features = feats;
    h->mt_metric_score = metric;
    return h;
  }

  void UpdateOracles(int sent_id, const Hypergraph& forest) {
    //shared_ptr<HypothesisInfo>& cur_ref = oracles[sent_id].good;
    cur_ref = oracles[sent_id].good; 
    if(!best_ever)
      cur_ref.reset();

    KBest::KBestDerivations<vector<WordID>, ESentenceTraversal> kbest(forest, kbest_size);
    double costaug_best_score = 0;

    for (int i = 0; i < kbest_size; ++i) {
      const KBest::KBestDerivations<vector<WordID>, ESentenceTraversal>::Derivation* d =
        kbest.LazyKthBest(forest.nodes_.size() - 1, i);
      if (!d) break;
      double mt_metric_score = ds[sent_id]->ScoreCandidate(d->yield)->ComputeScore(); //this might need to change!!
      const SparseVector<double>& feature_vals = d->feature_values; 
      double costaugmented_score = cost_augmented_score(d->score, mt_metric_score, mt_metric_scale, log_bleu); //note that d->score, i.e., model score, is passed in
      if (i == 0) { //i.e., setting up cur_best to be model score highest, and initializing costaug_best
        cur_best = MakeHypothesisInfo(feature_vals, mt_metric_score);
        cur_costaug_best = cur_best;
        costaug_best_score = costaugmented_score; 
      }
      if (costaugmented_score > costaug_best_score) {   // kbest_mira's cur_bad, i.e., "fear" derivation
        cur_costaug_best = MakeHypothesisInfo(feature_vals, mt_metric_score);
        costaug_best_score = costaugmented_score;
      }
      double cur_muscore = mt_metric_score;
      if (!cur_ref)   // kbest_mira's cur_good, i.e., "hope" derivation
        cur_ref =  MakeHypothesisInfo(feature_vals, cur_muscore);
      else {
          double cur_ref_muscore = cur_ref->mt_metric_score;
          if(mu > 0) { //select oracle with mixture of model score and BLEU
              cur_ref_muscore =  muscore(feature_weights, cur_ref->features, cur_ref->mt_metric_score, mu, log_bleu);
              cur_muscore = muscore(feature_weights, d->feature_values, mt_metric_score, mu, log_bleu);
          }
          if (cur_muscore > cur_ref_muscore) //replace oracle
            cur_ref = MakeHypothesisInfo(feature_vals, mt_metric_score);
      }
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

  const bool best_ever = conf.count("best_ever") > 0;
  vector<string> corpus;
  ReadTrainingCorpus(conf["input"].as<string>(), &corpus);

  const string metric_name = conf["mt_metric"].as<string>(); //set up scoring; this may need to be changed!!
  
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

  ReadFile ini_rf(conf["decoder_config"].as<string>());
  Decoder decoder(ini_rf.stream());

  // load initial weights
  vector<weight_t>& decoder_weights = decoder.CurrentWeightVector(); //equivalent to "dense_weights" vector in kbest_mira.cc
  SparseVector<weight_t> sparse_weights; //equivaelnt to  kbest_mira.cc "lambdas"
  Weights::InitFromFile(conf["weights"].as<string>(), &decoder_weights);
  Weights::InitSparseVector(decoder_weights, &sparse_weights);

  //initializing other algorithm and output parameters
  const double c = conf["regularizer_strength"].as<double>();
  const int weights_write_interval = conf["weights_write_interval"].as<int>();
  const double mt_metric_scale = conf["mt_metric_scale"].as<double>();
  const double mu = conf["mu"].as<double>();
  const double metric_threshold = conf["metric_threshold"].as<double>();
  const double stepsize_param = conf["stepsize_param"].as<double>(); //step size in structured SGD optimization step
  const bool stepsize_reduce = conf.count("stepsize_reduce") > 0; 
  const bool costaug_log_bleu = conf.count("costaug_log_bleu") > 0;
  const bool average = conf.count("average") > 0;
  const bool checkpositive = conf.count("check_positive") > 0;

  assert(corpus.size() > 0);
  vector<GoodOracle> oracles(corpus.size());
  TrainingObserver observer(conf["k_best_size"].as<int>(),  // kbest size
                            ds,                             // doc scorer
                            &oracles,
                            decoder_weights,
                            mt_metric_scale,
                            mu,
                            best_ever,
                            costaug_log_bleu);
  int cur_sent = 0;
  int line_count = 0;
  int normalizer = 0; 
  double total_loss = 0;
  double prev_loss = 0;
  int dots = 0;             // progess bar
  int cur_pass = 0;
  SparseVector<double> tot;
  tot += sparse_weights; //add initial weights to total
  normalizer++; //add 1 to normalizer
  int max_iteration = conf["passes"].as<int>();
  string msg = "# LatentSVM tuned weights";
  vector<int> order;
  int interval_counter = 0;
  RandomPermutation(corpus.size(), &order); //shuffle corpus
  while (line_count <= max_iteration * corpus.size()) { //loop over all (passes * num sentences) examples
    //if ((interval_counter * 40 / weights_write_interval) > dots) { ++dots; cerr << '.'; } //check this
    if ((cur_sent * 40 / corpus.size()) > dots) { ++dots; cerr << '.';}
    if (interval_counter == weights_write_interval) { //i.e., we need to write out weights
      sparse_weights *= scaling_trick;
      tot *= scaling_trick;
      scaling_trick = 1;
      cerr << " [SENTENCE NUMBER= " << cur_sent << "\n";
      cerr << " [AVG METRIC LAST INTERVAL =" << ((total_loss - prev_loss) / weights_write_interval) << "]\n";
      cerr << " [AVG METRIC THIS PASS THUS FAR =" << (total_loss / cur_sent) << "]\n";
      cerr << " [TOTAL LOSS: =" << total_loss << "\n";
      Weights::ShowLargestFeatures(decoder_weights);
      //dots = 0;
      interval_counter = 0;
      prev_loss = total_loss;
      if (average){
	SparseVector<double> x = tot;
	x /= normalizer;
	ostringstream sa;
	sa << "weights.latentsvm-" << line_count/weights_write_interval << "-avg.gz";
	x.init_vector(&decoder_weights);
	Weights::WriteToFile(sa.str(), decoder_weights, true, &msg); 
      }
      else {
	ostringstream os;
	os << "weights.latentsvm-" << line_count/weights_write_interval << ".gz";
	sparse_weights.init_vector(&decoder_weights);
	Weights::WriteToFile(os.str(), decoder_weights, true, &msg);
      }
    }
    if (corpus.size() == cur_sent) { //i.e., finished a pass
      //cerr << " [AVG METRIC LAST PASS=" << (document_metric_score / corpus.size()) << "]\n";
      cerr << " [AVG METRIC LAST PASS=" << (total_loss / corpus.size()) << "]\n";
      cerr << " TOTAL LOSS: " << total_loss << "\n";
      Weights::ShowLargestFeatures(decoder_weights);
      cur_sent = 0;
      total_loss = 0;
      dots = 0;
      if(average) {
        SparseVector<double> x = tot; 
        x /= normalizer;
        ostringstream sa;
        sa << "weights.latentsvm-pass" << (cur_pass < 10 ? "0" : "") << cur_pass << "-avg.gz";
        x.init_vector(&decoder_weights);
        Weights::WriteToFile(sa.str(), decoder_weights, true, &msg);
      }
      else {
	ostringstream os;
	os << "weights.latentsvm-pass" << (cur_pass < 10 ? "0" : "") << cur_pass << ".gz";
	Weights::WriteToFile(os.str(), decoder_weights, true, &msg);	
      }
      cur_pass++;
      RandomPermutation(corpus.size(), &order);
    }
    if (cur_sent == 0) { //i.e., starting a new pass
      cerr << "PASS " << (line_count / corpus.size() + 1) << endl;
    }
    sparse_weights.init_vector(&decoder_weights);   // copy sparse_weights to the decoder weights
    decoder.SetId(order[cur_sent]); //assign current sentence
    decoder.Decode(corpus[order[cur_sent]], &observer);  // decode/update oracles

    const HypothesisInfo& cur_best = observer.GetCurrentBestHypothesis(); //model score best
    const HypothesisInfo& cur_costaug = observer.GetCurrentCostAugmentedHypothesis(); //(model + cost) best; cost = -metric_scale*log(BLEU) or -metric_scale*BLEU
    //const HypothesisInfo& cur_ref = *oracles[order[cur_sent]].good; //this oracle-best line only picks based on BLEU
    const HypothesisInfo& cur_ref = observer.GetCurrentReference();  //if mu > 0, this mu-mixed oracle will be picked; otherwise, only on BLEU
    total_loss += cur_best.mt_metric_score; 

    double step_size = stepsize_param;
    if (stepsize_reduce){       // w_{t+1} = w_t - stepsize_t * grad(Loss) 
        step_size  /= (sqrt(cur_sent+1.0)); 
    }
    //actual update step - compute gradient, and modify sparse_weights
    if(cur_ref.mt_metric_score - cur_costaug.mt_metric_score > metric_threshold) {
      const double loss = (cur_costaug.features.dot(decoder_weights) - cur_ref.features.dot(decoder_weights)) * scaling_trick + mt_metric_scale * (cur_ref.mt_metric_score - cur_costaug.mt_metric_score);
      if (!checkpositive || loss > 0.0) { //can update either all the time if check positive is off, or only when loss > 0 if it's on
	sparse_weights -= cur_costaug.features * step_size / ((1.0-2.0*step_size*c)*scaling_trick);    // cost augmented hyp orig -
	sparse_weights += cur_ref.features * step_size / ((1.0-2.0*step_size*c)*scaling_trick);        // ref orig +
      }
    }
    scaling_trick *= (1.0 - 2.0 * step_size * c);

    tot += sparse_weights; //for averaging purposes
    normalizer++; //for averaging purposes
    line_count++;
    interval_counter++;
    cur_sent++;
  }
  cerr << endl;
  if(average) {
    tot /= normalizer;
    tot.init_vector(decoder_weights);
    msg = "# Latent SSVM tuned weights (averaged vector)";
    Weights::WriteToFile("weights.latentsvm-final-avg.gz", decoder_weights, true, &msg); 
    cerr << "Optimization complete.\n" << "AVERAGED WEIGHTS: weights.latentsvm-final-avg.gz\n";
  } else {
    Weights::WriteToFile("weights.latentsvm-final.gz", decoder_weights, true, &msg);    
    cerr << "Optimization complete.\n";
  }
  return 0;
}

