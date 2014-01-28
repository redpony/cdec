#include <sstream>
#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>
#include <algorithm>

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
#include "time.h"
#include "sampler.h"

#include "weights.h"
#include "sparse_vector.h"

using namespace std;
namespace po = boost::program_options;

bool invert_score;
boost::shared_ptr<MT19937> rng;
bool approx_score;
bool no_reweight;
bool no_select;
bool unique_kbest;
int update_list_size;
vector<weight_t> dense_w_local;
double mt_metric_scale;
int optimizer;
int fear_select;
int hope_select;
bool pseudo_doc;
bool sent_approx;
bool checkloss;
bool stream;

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
    ("pass,p", po::value<int>()->default_value(15), "Current pass through the training data")
    ("reference,r",po::value<vector<string> >(), "[REQD] Reference translation(s) (tokenized text file)")
    ("mt_metric,m",po::value<string>()->default_value("ibm_bleu"), "Scoring metric (ibm_bleu, nist_bleu, koehn_bleu, ter, combi)")
    ("optimizer,o",po::value<int>()->default_value(1), "Optimizer (SGD=1, PA MIRA w/Delta=2, Cutting Plane MIRA=3, PA MIRA=4, Triple nbest list MIRA=5)")
    ("fear,f",po::value<int>()->default_value(1), "Fear selection (model-cost=1, maxcost=2, maxscore=3)")
    ("hope,h",po::value<int>()->default_value(1), "Hope selection (model+cost=1, mincost=2)")
    ("max_step_size,C", po::value<double>()->default_value(0.01), "regularization strength (C)")
    ("random_seed,S", po::value<uint32_t>(), "Random seed (if not specified, /dev/random will be used)")
    ("mt_metric_scale,s", po::value<double>()->default_value(1.0), "Amount to scale MT loss function by")
    ("sent_approx,a", "Use smoothed sentence-level BLEU score for approximate scoring")
    ("pseudo_doc,e", "Use pseudo-document BLEU score for approximate scoring")
    ("no_reweight,d","Do not reweight forest for cutting plane")
    ("no_select,n", "Do not use selection heuristic")
    ("k_best_size,k", po::value<int>()->default_value(250), "Size of hypothesis list to search for oracles")
    ("update_k_best,b", po::value<int>()->default_value(1), "Size of good, bad lists to perform update with")
    ("unique_k_best,u", "Unique k-best translation list")
    ("stream,t", "Stream mode (used for realtime)")
    ("weights_output,O",po::value<string>(),"Directory to write weights to")
    ("output_dir,D",po::value<string>(),"Directory to place output in")
    ("decoder_config,c",po::value<string>(),"Decoder configuration file");
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
          || !conf->count("input_weights")
          || !conf->count("decoder_config")
          || (!conf->count("stream") && (!conf->count("reference") || !conf->count("weights_output") || !conf->count("output_dir")))
          ) {
    cerr << dcmdline_options << endl;
    return false;
  }
  return true;
}

//load previous translation, store array of each sentences score, subtract it from current sentence and replace with new translation score


static const double kMINUS_EPSILON = -1e-6;
static const double EPSILON = 0.000001;
static const double SMO_EPSILON = 0.0001;
static const double PSEUDO_SCALE = 0.95;
static const int MAX_SMO = 10;
int cur_pass;

struct HypothesisInfo {
  HypothesisInfo() : mt_metric(), hope(), fear(), alpha(), oracle_loss() {}
  SparseVector<double> features;
  vector<WordID> hyp;
  double mt_metric;
  double hope;
  double fear;
  double alpha;
  double oracle_loss;
  SparseVector<double> oracle_feat_diff;
  boost::shared_ptr<HypothesisInfo> oracleN;
};

bool ApproxEqual(double a, double b) {
  if (a == b) return true;
  return (fabs(a-b)/fabs(b)) < EPSILON;
}

typedef boost::shared_ptr<HypothesisInfo> HI;
bool HypothesisCompareB(const HI& h1, const HI& h2 ) 
{
  return h1->mt_metric > h2->mt_metric;
};


bool HopeCompareB(const HI& h1, const HI& h2 ) 
{
  return h1->hope > h2->hope;
};

bool FearCompareB(const HI& h1, const HI& h2 ) 
{
  return h1->fear > h2->fear;
};

bool FearComparePred(const HI& h1, const HI& h2 ) 
{
  return h1->features.dot(dense_w_local) > h2->features.dot(dense_w_local);
};

bool HypothesisCompareG(const HI& h1, const HI& h2 ) 
{
  return h1->mt_metric < h2->mt_metric;
};


void CuttingPlane(vector<boost::shared_ptr<HypothesisInfo> >* cur_c, bool* again, vector<boost::shared_ptr<HypothesisInfo> >& all_hyp, vector<weight_t> dense_weights)
{
  bool DEBUG_CUT = false;
  boost::shared_ptr<HypothesisInfo> max_fear, max_fear_in_set;
  vector<boost::shared_ptr<HypothesisInfo> >& cur_constraint = *cur_c;

  if(no_reweight)
    {
      //find new hope hypothesis
      for(int u=0;u!=all_hyp.size();u++)	
	{ 
	  double t_score = all_hyp[u]->features.dot(dense_weights);
	  all_hyp[u]->hope = 1 * all_hyp[u]->mt_metric + t_score;
	}
      
      //sort hyps by hope score
      sort(all_hyp.begin(),all_hyp.end(),HopeCompareB);    
      
      double hope_score = all_hyp[0]->features.dot(dense_weights);
      if(DEBUG_CUT) cerr << "New hope derivation score " << hope_score << endl;
     
      for(int u=0;u!=all_hyp.size();u++)	
	{ 
	  double t_score = all_hyp[u]->features.dot(dense_weights);
	  all_hyp[u]->fear = -1*all_hyp[u]->mt_metric + 1*all_hyp[0]->mt_metric - hope_score + t_score; //relative loss
	}
    
      sort(all_hyp.begin(),all_hyp.end(),FearCompareB);
      
    }
  //assign maximum fear derivation from all derivations
  max_fear = all_hyp[0];
  
  if(DEBUG_CUT) cerr <<"Cutting Plane Max Fear "<<max_fear->fear ;
  for(int i=0; i < cur_constraint.size();i++) //select maximal violator already in constraint set
    {
      if (!max_fear_in_set || cur_constraint[i]->fear > max_fear_in_set->fear)
	max_fear_in_set = cur_constraint[i];
    }
  if(DEBUG_CUT) cerr << "Max Fear in constraint set " << max_fear_in_set->fear << endl;
  
  if(max_fear->fear > max_fear_in_set->fear + SMO_EPSILON)
    {
      cur_constraint.push_back(max_fear);
      *again = true;
      if(DEBUG_CUT) cerr << "Optimize Again " << *again << endl;
    }
}


double ComputeDelta(vector<boost::shared_ptr<HypothesisInfo> >* cur_p, double max_step_size,vector<weight_t> dense_weights )
{
  vector<boost::shared_ptr<HypothesisInfo> >& cur_pair = *cur_p;
   double loss = cur_pair[0]->oracle_loss - cur_pair[1]->oracle_loss;

   double margin = -(cur_pair[0]->oracleN->features.dot(dense_weights)- cur_pair[0]->features.dot(dense_weights)) + (cur_pair[1]->oracleN->features.dot(dense_weights) - cur_pair[1]->features.dot(dense_weights));
   const double num = margin +  loss;
   cerr << "LOSS: " << num << " Margin:" << margin << " BLEUL:" << loss << " " << cur_pair[1]->features.dot(dense_weights) << " " << cur_pair[0]->features.dot(dense_weights) <<endl;
   

  SparseVector<double> diff = cur_pair[0]->features;
  diff -= cur_pair[1]->features;
  double diffsqnorm = diff.l2norm_sq();
  double delta;
  if (diffsqnorm > 0)
    delta = num / (diffsqnorm * max_step_size);
  else
    delta = 0;
  cerr << " D1:" << delta;
  //clip delta (enforce margin constraints)
  delta = max(-cur_pair[0]->alpha, min(delta, cur_pair[1]->alpha));
  cerr << " D2:" << delta;
  return delta;
}


vector<boost::shared_ptr<HypothesisInfo> > SelectPair(vector<boost::shared_ptr<HypothesisInfo> >* cur_c)
{
  bool DEBUG_SELECT= false;
  vector<boost::shared_ptr<HypothesisInfo> >& cur_constraint = *cur_c;
  
  vector<boost::shared_ptr<HypothesisInfo> > pair;

  if (no_select || optimizer == 2){ //skip heuristic search and return oracle and fear for pa-mira

      pair.push_back(cur_constraint[0]);
      pair.push_back(cur_constraint[1]);
      return pair;

    }
  
  for(int u=0;u != cur_constraint.size();u++)	
    {
      boost::shared_ptr<HypothesisInfo> max_fear;
      
      if(DEBUG_SELECT) cerr<< "cur alpha " << u  << " " << cur_constraint[u]->alpha;
      for(int i=0; i < cur_constraint.size();i++) //select maximal violator
	{
	  if(i != u)
	    if (!max_fear || cur_constraint[i]->fear > max_fear->fear)
	      max_fear = cur_constraint[i];
	}
      if(!max_fear) return pair; //
      
      
      if ((cur_constraint[u]->alpha == 0) && (cur_constraint[u]->fear > max_fear->fear + SMO_EPSILON))
	{
	  for(int i=0; i < cur_constraint.size();i++) //select maximal violator
	    {
	      if(i != u)
		if (cur_constraint[i]->alpha > 0)
		  {
		    pair.push_back(cur_constraint[u]);
		    pair.push_back(cur_constraint[i]);		    
		    return pair;
		  }
	    }
	}	       
      if ((cur_constraint[u]->alpha > 0) && (cur_constraint[u]->fear < max_fear->fear - SMO_EPSILON))
	{
	  for(int i=0; i < cur_constraint.size();i++) //select maximal violator
	    {
	      if(i != u)	
		if (cur_constraint[i]->fear > cur_constraint[u]->fear)
		  {
		    pair.push_back(cur_constraint[u]);
		    pair.push_back(cur_constraint[i]);
		    return pair;
		  }
	    }  
	}
    
    } 
  return pair; //no more constraints to optimize, we're done here

}

struct GoodBadOracle {
  vector<boost::shared_ptr<HypothesisInfo> > good;
  vector<boost::shared_ptr<HypothesisInfo> > bad;
};

struct BasicObserver: public DecoderObserver {
    Hypergraph* hypergraph;
    BasicObserver() : hypergraph(NULL) {}
    ~BasicObserver() {
        if(hypergraph != NULL) delete hypergraph;
    }
    void NotifyDecodingStart(const SentenceMetadata& smeta) {}
    void NotifySourceParseFailure(const SentenceMetadata& smeta) {}
    void NotifyTranslationForest(const SentenceMetadata& smeta, Hypergraph* hg) {
        if(hypergraph != NULL) delete hypergraph;
        hypergraph = new Hypergraph(*hg);
    }
    void NotifyAlignmentFailure(const SentenceMetadata& semta) {
        if(hypergraph != NULL) delete hypergraph;
    }
    void NotifyAlignmentForest(const SentenceMetadata& smeta, Hypergraph* hg) {}
    void NotifyDecodingComplete(const SentenceMetadata& smeta) {}
};

struct TrainingObserver : public DecoderObserver {
  TrainingObserver(const int k, const DocScorer& d, vector<GoodBadOracle>* o, vector<ScoreP>* cbs) : ds(d), oracles(*o), corpus_bleu_sent_stats(*cbs), kbest_size(k) {
    

    if(!pseudo_doc && !sent_approx)
    if(cur_pass > 0)     //calculate corpus bleu score from previous iterations 1-best for BLEU gain
      {
	ScoreP acc;
	for (int ii = 0; ii < corpus_bleu_sent_stats.size(); ii++) {
	  if (!acc) { acc = corpus_bleu_sent_stats[ii]->GetZero(); }
	  acc->PlusEquals(*corpus_bleu_sent_stats[ii]);
	  
	}
	corpus_bleu_stats = acc;
	corpus_bleu_score = acc->ComputeScore();
      }

}
  const DocScorer& ds;
  vector<ScoreP>& corpus_bleu_sent_stats;
  vector<GoodBadOracle>& oracles;
  vector<boost::shared_ptr<HypothesisInfo> > cur_best;
  boost::shared_ptr<HypothesisInfo> cur_oracle;
  const int kbest_size;
  Hypergraph forest;
  int cur_sent;
  ScoreP corpus_bleu_stats;
  float corpus_bleu_score;

  float corpus_src_length;
  float curr_src_length;

  const int GetCurrentSent() const {
    return cur_sent;
  }

  const HypothesisInfo& GetCurrentBestHypothesis() const {
    return *cur_best[0];
  }

  const vector<boost::shared_ptr<HypothesisInfo> > GetCurrentBest() const {
    return cur_best;
  }
  
 const HypothesisInfo& GetCurrentOracle() const {
    return *cur_oracle;
  }
  
  const Hypergraph& GetCurrentForest() const {
    return forest;
  }
  

  virtual void NotifyTranslationForest(const SentenceMetadata& smeta, Hypergraph* hg) {
    cur_sent = stream ? 0 : smeta.GetSentenceID();
    curr_src_length = (float) smeta.GetSourceLength();

    if(unique_kbest)
      UpdateOracles<KBest::FilterUnique>(smeta.GetSentenceID(), *hg);
    else
      UpdateOracles<KBest::NoFilter<std::vector<WordID> > >(smeta.GetSentenceID(), *hg);
    forest = *hg;
    
  }

  boost::shared_ptr<HypothesisInfo> MakeHypothesisInfo(const SparseVector<double>& feats, const double score, const vector<WordID>& hyp) {
    boost::shared_ptr<HypothesisInfo> h(new HypothesisInfo);
    h->features = feats;
    h->mt_metric = score;
    h->hyp = hyp;
    return h;
  }

  template <class Filter>  
  void UpdateOracles(int sent_id, const Hypergraph& forest) {

    if (stream) sent_id = 0;
    bool PRINT_LIST= false;
    assert(sent_id < oracles.size());
    vector<boost::shared_ptr<HypothesisInfo> >& cur_good = oracles[sent_id].good;
    vector<boost::shared_ptr<HypothesisInfo> >& cur_bad = oracles[sent_id].bad;
    //TODO: look at keeping previous iterations hypothesis lists around
    cur_best.clear();
    cur_good.clear();
    cur_bad.clear();

    vector<boost::shared_ptr<HypothesisInfo> > all_hyp;

    typedef KBest::KBestDerivations<vector<WordID>, ESentenceTraversal,Filter> K;
    K kbest(forest,kbest_size);
    
    for (int i = 0; i < kbest_size; ++i) {

      typename K::Derivation *d =
        kbest.LazyKthBest(forest.nodes_.size() - 1, i);
      if (!d) break;

      float sentscore;
	  if(cur_pass > 0 && !pseudo_doc && !sent_approx)
	    {
	      ScoreP sent_stats = ds[sent_id]->ScoreCandidate(d->yield);
	      ScoreP corpus_no_best = corpus_bleu_stats->GetZero();

	      corpus_bleu_stats->Subtract(*corpus_bleu_sent_stats[sent_id], &*corpus_no_best);
	      sent_stats->PlusEquals(*corpus_no_best, 0.5);
	      
	      //compute gain from new sentence in 1-best corpus
	      sentscore = mt_metric_scale * (sent_stats->ComputeScore() - corpus_no_best->ComputeScore());// - corpus_bleu_score);
	    }
	  else if(pseudo_doc)   //pseudo-corpus smoothing 
	    {
	      float src_scale = corpus_src_length + curr_src_length;
	      ScoreP sent_stats = ds[sent_id]->ScoreCandidate(d->yield);
	      if(!corpus_bleu_stats){ corpus_bleu_stats = sent_stats->GetZero();}
	      
	      sent_stats->PlusEquals(*corpus_bleu_stats);
	      sentscore =  mt_metric_scale  * src_scale * sent_stats->ComputeScore();

	    }
	  else //use sentence-level smoothing ( used when cur_pass=0 if not pseudo_doc)
	    {
	     
	      sentscore = mt_metric_scale * (ds[sent_id]->ScoreCandidate(d->yield)->ComputeScore());
	    }
	
      if (invert_score) sentscore *= -1.0;
      
      if (i < update_list_size){ 
	if(PRINT_LIST)cerr << TD::GetString(d->yield) << " ||| " << d->score << " ||| " << sentscore << endl; 
	cur_best.push_back( MakeHypothesisInfo(d->feature_values, sentscore, d->yield));
      }
      
      all_hyp.push_back(MakeHypothesisInfo(d->feature_values, sentscore,d->yield));   //store all hyp to extract hope and fear         
    }
    
    if(pseudo_doc){
    //update psuedo-doc stats
      string details, details2;     
      corpus_bleu_stats->ScoreDetails(&details2);   
      ScoreP sent_stats = ds[sent_id]->ScoreCandidate(cur_best[0]->hyp);
      corpus_bleu_stats->PlusEquals(*sent_stats);
      
      sent_stats->ScoreDetails(&details);
      sent_stats = corpus_bleu_stats;
      corpus_bleu_stats = sent_stats->GetZero();
      corpus_bleu_stats->PlusEquals(*sent_stats, PSEUDO_SCALE);
            
      corpus_src_length = PSEUDO_SCALE * (corpus_src_length + curr_src_length);
      cerr << "ps corpus size: " << corpus_src_length << " " << curr_src_length << "\n" << details << "\n" << details2 << endl;
    }

    //figure out how many hyps we can keep maximum
    int temp_update_size = update_list_size;
    if (all_hyp.size() < update_list_size){ temp_update_size = all_hyp.size();}

    //sort all hyps by sentscore (eg. bleu)
    sort(all_hyp.begin(),all_hyp.end(),HypothesisCompareB);
    
    if(PRINT_LIST){  cerr << "Sorting " << endl; for(int u=0;u!=all_hyp.size();u++)  
						   cerr << all_hyp[u]->mt_metric << " " << all_hyp[u]->features.dot(dense_w_local) << endl; }
    
    if(hope_select == 1)
      {
	//find hope hypothesis using model + bleu
	if (PRINT_LIST) cerr << "HOPE " << endl;
	for(int u=0;u!=all_hyp.size();u++)	
	  { 
	    double t_score = all_hyp[u]->features.dot(dense_w_local);
	    all_hyp[u]->hope = all_hyp[u]->mt_metric + t_score;
	    if (PRINT_LIST) cerr << all_hyp[u]->mt_metric << " H:" << all_hyp[u]->hope << " S:" << t_score << endl; 
	    
	  }
	
	//sort hyps by hope score
	sort(all_hyp.begin(),all_hyp.end(),HopeCompareB);
      }        

    //assign cur_good the sorted list
    cur_good.insert(cur_good.begin(), all_hyp.begin(), all_hyp.begin()+temp_update_size);    
    if(PRINT_LIST) { cerr << "GOOD" << endl;  for(int u=0;u!=cur_good.size();u++) cerr << cur_good[u]->mt_metric << " " << cur_good[u]->hope << endl;}     

    //use hope for fear selection
    boost::shared_ptr<HypothesisInfo>& oracleN = cur_good[0];

    if(fear_select == 1){   //compute fear hyps with model - bleu
      if (PRINT_LIST) cerr << "FEAR " << endl;
      double hope_score = oracleN->features.dot(dense_w_local);

      if (PRINT_LIST) cerr << "hope score " << hope_score << endl;
      for(int u=0;u!=all_hyp.size();u++)	
	{ 
	  double t_score = all_hyp[u]->features.dot(dense_w_local);

	  all_hyp[u]->fear = -1*all_hyp[u]->mt_metric + 1*oracleN->mt_metric - hope_score + t_score; //relative loss
	  all_hyp[u]->oracle_loss = -1*all_hyp[u]->mt_metric + 1*oracleN->mt_metric;
	  all_hyp[u]->oracle_feat_diff = oracleN->features - all_hyp[u]->features;
	  all_hyp[u]->oracleN=oracleN;
	  if (PRINT_LIST) cerr << all_hyp[u]->mt_metric << " H:" << all_hyp[u]->hope << " F:" << all_hyp[u]->fear << endl; 
	  
	}
      
      sort(all_hyp.begin(),all_hyp.end(),FearCompareB);
      
    }
    else if(fear_select == 2) //select fear based on cost
      {
	sort(all_hyp.begin(),all_hyp.end(),HypothesisCompareG);
      }
    else //max model score, also known as prediction-based
      {
	sort(all_hyp.begin(),all_hyp.end(),FearComparePred);
      }
    cur_bad.insert(cur_bad.begin(), all_hyp.begin(), all_hyp.begin()+temp_update_size); 

    if(PRINT_LIST){ cerr<< "BAD"<<endl; for(int u=0;u!=cur_bad.size();u++) cerr << cur_bad[u]->mt_metric << " H:" << cur_bad[u]->hope << " F:" << cur_bad[u]->fear << endl;}
    
    cerr << "GOOD (BEST): " << cur_good[0]->mt_metric << endl;
    cerr << " CUR: " << cur_best[0]->mt_metric << endl;
    cerr << " BAD (WORST): " << cur_bad[0]->mt_metric << endl;
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

void ReadPastTranslationForScore(const int cur_pass, vector<ScoreP>* c, DocScorer& ds, const string& od)
{
  cerr << "Reading BLEU gain file ";
  string fname;
  if(cur_pass == 0)
    {
      fname = od + "/run.raw.init";
    }
  else
    {
      int last_pass = cur_pass - 1; 
      fname = od + "/run.raw."  +  boost::lexical_cast<std::string>(last_pass) + ".B";
    }
  cerr << fname << "\n";
  ReadFile rf(fname);
  istream& in = *rf.stream();
  ScoreP acc;
  string line;
  int lc = 0;
  while(in) {
    getline(in, line);
    if (line.empty() && !in) break;
    vector<WordID> sent;
    TD::ConvertSentence(line, &sent);
    ScoreP sentscore = ds[lc]->ScoreCandidate(sent);
    c->push_back(sentscore);
    if (!acc) { acc = sentscore->GetZero(); }
    acc->PlusEquals(*sentscore);
    ++lc;
 
  }
  
  assert(lc > 0);
  float score = acc->ComputeScore();
  string details;
  acc->ScoreDetails(&details);
  cerr << "Previous run: " << details << score << endl;

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
  
  vector<string> corpus;

  const string metric_name = conf["mt_metric"].as<string>();
  optimizer = conf["optimizer"].as<int>();
  fear_select = conf["fear"].as<int>();
  hope_select = conf["hope"].as<int>();
  mt_metric_scale = conf["mt_metric_scale"].as<double>();
  approx_score = conf.count("approx_score");
  no_reweight = conf.count("no_reweight");
  no_select = conf.count("no_select");
  update_list_size = conf["update_k_best"].as<int>();
  unique_kbest = conf.count("unique_k_best");
  stream = conf.count("stream");
  pseudo_doc = conf.count("pseudo_doc");
  sent_approx = conf.count("sent_approx");
  cerr << "Using pseudo-doc:" << pseudo_doc << " Sent:" << sent_approx << endl;
  if(pseudo_doc)
    mt_metric_scale=1;

  const string weights_dir = stream ? "-" : conf["weights_output"].as<string>();
  const string output_dir = stream ? "-" : conf["output_dir"].as<string>();
  ScoreType type = ScoreTypeFromString(metric_name);

  //establish metric used for tuning
  if (type == TER) {
    invert_score = true;
  } else {
    invert_score = false;
  }

  boost::shared_ptr<DocScorer> ds;
  //normal: load references, stream: start stream scorer
  if (stream) {
	  ds = boost::shared_ptr<DocScorer>(new DocStreamScorer(type, vector<string>(0), ""));
	  cerr << "Scoring doc stream with " << metric_name << endl;
  } else {
      ds = boost::shared_ptr<DocScorer>(new DocScorer(type, conf["reference"].as<vector<string> >(), ""));
      cerr << "Loaded " << ds->size() << " references for scoring with " << metric_name << endl;
  }
  vector<ScoreP> corpus_bleu_sent_stats;
  
  //check training pass,if >0, then use previous iterations corpus bleu stats
  cur_pass = stream ? 0 : conf["pass"].as<int>();
  if(cur_pass > 0)
    {
      ReadPastTranslationForScore(cur_pass, &corpus_bleu_sent_stats, *ds, output_dir);
    }
  
  cerr << "Using optimizer:" << optimizer << endl;
    
  ReadFile ini_rf(conf["decoder_config"].as<string>());
  Decoder decoder(ini_rf.stream());

  vector<weight_t>& dense_weights = decoder.CurrentWeightVector();
  
  SparseVector<weight_t> lambdas;
  Weights::InitFromFile(conf["input_weights"].as<string>(), &dense_weights);
  Weights::InitSparseVector(dense_weights, &lambdas);

  const string input = stream ? "-" : decoder.GetConf()["input"].as<string>();
  if (!SILENT) cerr << "Reading input from " << ((input == "-") ? "STDIN" : input.c_str()) << endl;
  ReadFile in_read(input);
  istream *in = in_read.stream();
  assert(*in);  
  string buf;
  
  const double max_step_size = conf["max_step_size"].as<double>();

  vector<GoodBadOracle> oracles(ds->size());

  BasicObserver bobs;
  TrainingObserver observer(conf["k_best_size"].as<int>(), *ds, &oracles, &corpus_bleu_sent_stats);

  int cur_sent = 0;
  int lcount = 0;
  double objective=0;
  double tot_loss = 0;
  int dots = 0;
  SparseVector<double> tot;
  SparseVector<double> final_tot;

  SparseVector<double> old_lambdas = lambdas;
  tot.clear();
  tot += lambdas;
  cerr << "PASS " << cur_pass << " " << endl << lambdas << endl; 
  ScoreP acc, acc_h, acc_f;
  
  while(*in) {
      getline(*in, buf);
      if (buf.empty()) continue;
      if (stream) {
    	  cur_sent = 0;
    	  int delim = buf.find(" ||| ");
    	  // Translate only
    	  if (delim == -1) {
    		  decoder.SetId(cur_sent);
    		  decoder.Decode(buf, &bobs);
    		  vector<WordID> trans;
    		  ViterbiESentence(bobs.hypergraph[0], &trans);
    		  cout << TD::GetString(trans) << endl;
    		  continue;
          // Special command:
          // CMD ||| arg1 ||| arg2 ...
    	  } else {
              string cmd = buf.substr(0, delim);
              buf = buf.substr(delim + 5);
        	  // Translate and update (normal MIRA)
              // LEARN ||| source ||| reference
              if (cmd == "LEARN") {
                  delim = buf.find(" ||| ");
        		  ds->update(buf.substr(delim + 5));
        		  buf = buf.substr(0, delim);
              } else if (cmd == "WEIGHTS") {
                  // WEIGHTS ||| WRITE
                  if (buf == "WRITE") {
                      cout << Weights::GetString(dense_weights) << endl;
                  // WEIGHTS ||| f1=w1 f2=w2 ...
                  } else {
                      Weights::UpdateFromString(buf, dense_weights);
                  }
                  continue;
              } else {
                  cerr << "Error: cannot parse command, skipping line:" << endl;
                  cerr << cmd << " ||| " << buf << endl;
                  continue;
              }
    	  }
      }
      // Regular mode or LEARN line from stream mode
      //TODO: allow batch updating
      lambdas.init_vector(&dense_weights);
      dense_w_local = dense_weights;
      decoder.SetId(cur_sent);
      decoder.Decode(buf, &observer);  // decode the sentence, calling Notify to get the hope,fear, and model best hyps. 

      cur_sent = observer.GetCurrentSent();
      cerr << "SENT: " << cur_sent << endl;
      const HypothesisInfo& cur_hyp = observer.GetCurrentBestHypothesis();
      const HypothesisInfo& cur_good = *oracles[cur_sent].good[0];
      const HypothesisInfo& cur_bad = *oracles[cur_sent].bad[0];

      vector<boost::shared_ptr<HypothesisInfo> >& cur_good_v = oracles[cur_sent].good;
      vector<boost::shared_ptr<HypothesisInfo> >& cur_bad_v = oracles[cur_sent].bad;
      vector<boost::shared_ptr<HypothesisInfo> > cur_best_v = observer.GetCurrentBest();

      tot_loss += cur_hyp.mt_metric;
      
      //score hyps to be able to compute corpus level bleu after we finish this iteration through the corpus
      ScoreP sentscore = (*ds)[cur_sent]->ScoreCandidate(cur_hyp.hyp);
      if (!acc) { acc = sentscore->GetZero(); }
      acc->PlusEquals(*sentscore);

      ScoreP hope_sentscore = (*ds)[cur_sent]->ScoreCandidate(cur_good.hyp);
      if (!acc_h) { acc_h = hope_sentscore->GetZero(); }
      acc_h->PlusEquals(*hope_sentscore);

      ScoreP fear_sentscore = (*ds)[cur_sent]->ScoreCandidate(cur_bad.hyp);
      if (!acc_f) { acc_f = fear_sentscore->GetZero(); }
      acc_f->PlusEquals(*fear_sentscore);
      
      if(optimizer == 4) { //passive-aggresive update (single dual coordinate step)
      
	  double margin = cur_bad.features.dot(dense_weights) - cur_good.features.dot(dense_weights);
	  double mt_loss = (cur_good.mt_metric - cur_bad.mt_metric);
	  const double loss = margin +  mt_loss;
	  cerr << "LOSS: " << loss << " Margin:" << margin << " BLEUL:" << mt_loss << " " << cur_bad.features.dot(dense_weights) << " " << cur_good.features.dot(dense_weights) <<endl;
	  if (loss > 0.0 || !checkloss) {
	    SparseVector<double> diff = cur_good.features;
	    diff -= cur_bad.features;	    

	    double diffsqnorm = diff.l2norm_sq();
	    double delta;
	    if (diffsqnorm > 0)
	      delta = loss / (diffsqnorm);
	    else
	      delta = 0;
	    
	    if (delta > max_step_size) delta = max_step_size;
	    lambdas += (cur_good.features * delta);
	    lambdas -= (cur_bad.features * delta);
	    
	  }
      }
      else if(optimizer == 1) //sgd - nonadapted step size
	{
	  lambdas += (cur_good.features) * max_step_size;
	  lambdas -= (cur_bad.features) * max_step_size;
	}
      else if(optimizer == 5) //full mira with n-best list of constraints from hope, fear, model best
	{
	  vector<boost::shared_ptr<HypothesisInfo> > cur_constraint;
	  cur_constraint.insert(cur_constraint.begin(), cur_bad_v.begin(), cur_bad_v.end());
	  cur_constraint.insert(cur_constraint.begin(), cur_best_v.begin(), cur_best_v.end());
	  cur_constraint.insert(cur_constraint.begin(), cur_good_v.begin(), cur_good_v.end());

	  bool optimize_again;
	  vector<boost::shared_ptr<HypothesisInfo> > cur_pair;
	  //SMO 
	  for(int u=0;u!=cur_constraint.size();u++)	
	    cur_constraint[u]->alpha =0;	      
	  
	  cur_constraint[0]->alpha =1; //set oracle to alpha=1

	  cerr <<"Optimizing with " << cur_constraint.size() << " constraints" << endl;
	  int smo_iter = MAX_SMO, smo_iter2 = MAX_SMO;
	  int iter, iter2 =0;
	  bool DEBUG_SMO = false;
	  while (iter2 < smo_iter2)
	    {
	      iter =0;
	      while (iter < smo_iter)
		{
		  optimize_again = true;
		  for (int i = 0; i< cur_constraint.size(); i++)
		    for (int j = i+1; j< cur_constraint.size(); j++)
		      {
			if(DEBUG_SMO) cerr << "start " << i << " " << j <<  endl;
			cur_pair.clear();
			cur_pair.push_back(cur_constraint[j]);
			cur_pair.push_back(cur_constraint[i]);
			double delta = ComputeDelta(&cur_pair,max_step_size, dense_weights);
			
			if (delta == 0) optimize_again = false;
			cur_constraint[j]->alpha += delta;
			cur_constraint[i]->alpha -= delta;
			double step_size = delta * max_step_size;
			
			lambdas += (cur_constraint[i]->features) * step_size;
			lambdas -= (cur_constraint[j]->features) * step_size;
			if(DEBUG_SMO) cerr << "SMO opt " << iter << " " << i << " " << j << " " <<  delta << " " << cur_pair[0]->alpha << " " << cur_pair[1]->alpha <<  endl;		
		      }
		  iter++;
		  
		  if(!optimize_again)
		    { 
		      iter = MAX_SMO;
		      cerr << "Optimization stopped, delta =0" << endl;
		    }		  
		}
	      iter2++;
	    }	  
	}
      else if(optimizer == 2 || optimizer == 3) //PA and Cutting Plane MIRA update
	  {
	    bool DEBUG_SMO= true;
	    vector<boost::shared_ptr<HypothesisInfo> > cur_constraint;
	    cur_constraint.push_back(cur_good_v[0]); //add oracle to constraint set
	    bool optimize_again = true;
	    int cut_plane_calls = 0;
	    while (optimize_again)
	      { 
		if(DEBUG_SMO) cerr<< "optimize again: " << optimize_again << endl;
		if(optimizer == 2){ //PA
		  cur_constraint.push_back(cur_bad_v[0]);

		  //check if we have a violation
		  if(!(cur_constraint[1]->fear > cur_constraint[0]->fear + SMO_EPSILON))
		    {
		      optimize_again = false;
		      cerr << "Constraint not violated" << endl;
		    }
		}
		else
		  { //cutting plane to add constraints
		    if(DEBUG_SMO) cerr<< "Cutting Plane " << cut_plane_calls << " with " << lambdas << endl;
		    optimize_again = false;
		    cut_plane_calls++;
		    CuttingPlane(&cur_constraint, &optimize_again, oracles[cur_sent].bad, dense_weights);
		    if (cut_plane_calls >= MAX_SMO) optimize_again = false;
		  }

		if(optimize_again)
		  {
		    //SMO 
		    for(int u=0;u!=cur_constraint.size();u++)	
		      { 
			cur_constraint[u]->alpha =0;
		      }
		    cur_constraint[0]->alpha = 1;
		    cerr <<" Optimizing with " << cur_constraint.size() << " constraints" << endl;
		    int smo_iter = MAX_SMO;
		    int iter =0;
		    while (iter < smo_iter)
		      {			
			//select pair to optimize from constraint set
			vector<boost::shared_ptr<HypothesisInfo> > cur_pair = SelectPair(&cur_constraint);
			
			if(cur_pair.empty()){
			  iter=MAX_SMO; 
			  cerr << "Undefined pair " << endl; 
			  continue;
			} //pair is undefined so we are done with this smo 

			double delta = ComputeDelta(&cur_pair,max_step_size, dense_weights);

			cur_pair[0]->alpha += delta;
			cur_pair[1]->alpha -= delta;
			double step_size = delta * max_step_size;
			cerr << "step " << step_size << endl;

			lambdas += (cur_pair[1]->features) * step_size;
			lambdas -= (cur_pair[0]->features) * step_size;

			//reload weights based on update
			dense_weights.clear();
			lambdas.init_vector(&dense_weights);
                        if (dense_weights.size() < 500)
                          ShowLargestFeatures(dense_weights);
			dense_w_local = dense_weights;
			iter++;
					
			if(DEBUG_SMO) cerr << "SMO opt " << iter << " " << delta << " " << cur_pair[0]->alpha << " " << cur_pair[1]->alpha <<  endl;		
			if(no_select) //don't use selection heuristic to determine when to stop SMO, rather just when delta =0 
			  if (delta == 0) iter = MAX_SMO;
			
			//only perform one dual coordinate ascent step
			if(optimizer == 2) 
			  {
			    optimize_again = false;
			    iter = MAX_SMO;
			  }					
		      }
		    if(optimizer == 3)
		      {
			if(!no_reweight) //reweight the forest and select a new k-best
			  {
			    if(DEBUG_SMO) cerr<< "Decoding with new weights -- now orac are " << oracles[cur_sent].good.size() << endl;
			    Hypergraph hg = observer.GetCurrentForest();
			    hg.Reweight(dense_weights);
			    if(unique_kbest)
                              observer.UpdateOracles<KBest::FilterUnique>(cur_sent, hg);
                            else
                              observer.UpdateOracles<KBest::NoFilter<std::vector<WordID> > >(cur_sent, hg);			    
			  }
		      }
		  }
		
	      }
	   
	    //print objective after this sentence
	    double lambda_change = (lambdas - old_lambdas).l2norm_sq();
	    double max_fear = cur_constraint[cur_constraint.size()-1]->fear;
	    double temp_objective = 0.5 * lambda_change;// + max_step_size * max_fear;

	    for(int u=0;u!=cur_constraint.size();u++)	
	      { 
		cerr << "alpha=" << cur_constraint[u]->alpha << " hope=" << cur_constraint[u]->hope << " fear=" << cur_constraint[u]->fear << endl;
		temp_objective += cur_constraint[u]->alpha * cur_constraint[u]->fear;
	      }
	    objective += temp_objective;
	    
	    cerr << "SENT OBJ: " << temp_objective << " NEW OBJ: " << objective << endl;
	  }
      
    
      if ((cur_sent * 40 / ds->size()) > dots) { ++dots; cerr << '.'; }
      tot += lambdas;
      ++lcount;
      cur_sent++;

      cout << TD::GetString(cur_good_v[0]->hyp) << " ||| " << TD::GetString(cur_best_v[0]->hyp) << " ||| " << TD::GetString(cur_bad_v[0]->hyp) << endl;

    }

    cerr << "FINAL OBJECTIVE: "<< objective << endl;
    final_tot += tot;
    cerr << "Translated " << lcount << " sentences " << endl;
    cerr << " [AVG METRIC LAST PASS=" << (tot_loss / lcount) << "]\n";
    tot_loss = 0;

    // Write weights unless streaming
    if (!stream) {
		int node_id = rng->next() * 100000;
		cerr << " Writing weights to " << node_id << endl;
		//Weights::ShowLargestFeatures(dense_weights);
		dots = 0;
		ostringstream os;
		os << weights_dir << "/weights.mira-pass" << (cur_pass < 10 ? "0" : "") << cur_pass << "." << node_id << ".gz";
		string msg = "# MIRA tuned weights ||| " + boost::lexical_cast<std::string>(node_id) + " ||| " + boost::lexical_cast<std::string>(lcount);
		lambdas.init_vector(&dense_weights);
		Weights::WriteToFile(os.str(), dense_weights, true, &msg);
    
		SparseVector<double> x = tot;
		x /= lcount+1;
		ostringstream sa;
		string msga = "# MIRA tuned weights AVERAGED ||| " + boost::lexical_cast<std::string>(node_id) + " ||| " + boost::lexical_cast<std::string>(lcount);
		sa << weights_dir << "/weights.mira-pass" << (cur_pass < 10 ? "0" : "") << cur_pass << "." << node_id << "-avg.gz";
		x.init_vector(&dense_weights);
		Weights::WriteToFile(sa.str(), dense_weights, true, &msga);
    }
    
    cerr << "Optimization complete.\n";
    return 0;
}

