#ifndef ORACLE_BLEU_H
#define ORACLE_BLEU_H

#include <sstream>
#include <iostream>
#include <string>
#include <vector>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>
#include "../vest/scorer.h"
#include "hg.h"
#include "ff_factory.h"
#include "ff_bleu.h"
#include "sparse_vector.h"
#include "viterbi.h"
#include "sentence_metadata.h"
#include "apply_models.h"
#include "kbest.h"
#include "timing_stats.h"
#include "sentences.h"

//TODO: put function impls into .cc
//TODO: disentangle
struct Translation {
  typedef std::vector<WordID> Sentence;
  Sentence sentence;
  FeatureVector features;
  Translation() {  }
  Translation(Hypergraph const& hg,WeightVector *feature_weights=0)
  {
    Viterbi(hg,feature_weights);
  }
  void Viterbi(Hypergraph const& hg,WeightVector *feature_weights=0) // weights are only for checking that scoring is correct
  {
    ViterbiESentence(hg,&sentence);
    features=ViterbiFeatures(hg,feature_weights,true);
  }
  void Print(std::ostream &out,std::string pre="   +Oracle BLEU ") {
    out<<pre<<"Viterbi: "<<TD::GetString(sentence)<<"\n";
    out<<pre<<"features: "<<features<<std::endl;
  }
  bool is_null() {
    return features.empty() /* && sentence.empty() */;
  }

};

struct Oracle {
  bool is_null() {
    return model.is_null() /* && fear.is_null() && hope.is_null() */;
  }

  Translation model,fear,hope;
  // feature 0 will be the error rate in fear and hope
  // move toward hope
  FeatureVector ModelHopeGradient() const {
    FeatureVector r=hope.features-model.features;
    r.set_value(0,0);
    return r;
  }
  // move toward hope from fear
  FeatureVector FearHopeGradient() const {
    FeatureVector r=hope.features-fear.features;
    r.set_value(0,0);
    return r;
  }
};


struct OracleBleu {
  typedef std::vector<std::string> Refs;
  Refs refs;
  WeightVector feature_weights_;
  DocScorer ds;

  static void AddOptions(boost::program_options::options_description *opts) {
    using namespace boost::program_options;
    using namespace std;
    opts->add_options()
      ("references,R", value<Refs >(), "Translation reference files")
      ("oracle_loss", value<string>(), "IBM_BLEU_3 (default), IBM_BLEU etc")
      ("bleu_weight", value<double>()->default_value(1.), "weight to give the hope/fear loss function vs. model score")
      ;
  }
  int order;

  //TODO: move cdec.cc kbest output files function here

  //TODO: provide for loading most recent translation for every sentence (no more scale.. etc below? it's possible i messed the below up; i assume it's supposed to gracefully figure out the document 1bests as you go, then keep them up to date as you make multiple MIRA passes.  provide alternative loading for MERT
  double scale_oracle;
  int oracle_doc_size;
  double tmp_src_length;
  double doc_src_length;
  void set_oracle_doc_size(int size) {
    oracle_doc_size=size;
    scale_oracle=  1-1./oracle_doc_size;
    doc_src_length=0;
  }
  OracleBleu(int doc_size=10) {
    set_oracle_doc_size(doc_size);
  }

  typedef boost::shared_ptr<Score> ScoreP;
  ScoreP doc_score,sentscore; // made from factory, so we delete them
  ScoreP GetScore(Sentence const& sentence,int sent_id) {
    return ScoreP(ds[sent_id]->ScoreCandidate(sentence));
  }
  ScoreP GetScore(Hypergraph const& forest,int sent_id) {
    return GetScore(Translation(forest).sentence,sent_id);
  }

  double bleu_weight;
  void UseConf(boost::program_options::variables_map const& conf) {
    using namespace std;
    bleu_weight=conf["bleu_weight"].as<double>();
    set_loss(conf["oracle_loss"].as<string>());
    set_refs(conf["references"].as<Refs>());
  }

  ScoreType loss;
//  std::string loss_name;
  boost::shared_ptr<FeatureFunction> pff;

  void set_loss(std::string const& lossd="IBM_BLEU_3") {
//    loss_name=lossd;
    loss=ScoreTypeFromString(lossd);
    order=(loss==IBM_BLEU_3)?3:4;
    std::ostringstream param;
    param<<"-o "<<order;
    pff=global_ff_registry->Create("BLEUModel",param.str());
  }

  void set_refs(Refs const& r) {
    refs=r;
    assert(refs.size());
    ds=DocScorer(loss,refs);
    doc_score.reset();
//    doc_score=sentscore
    std::cerr << "Loaded " << ds.size() << " references for scoring with " << StringFromScoreType(loss) << std::endl;
  }

  SentenceMetadata MakeMetadata(Hypergraph const& forest,int sent_id) {
    std::vector<WordID> srcsent;
    ViterbiFSentence(forest,&srcsent);
    SentenceMetadata smeta(sent_id,Lattice()); //TODO: make reference from refs?
    smeta.SetSourceLength(srcsent.size());
	smeta.SetScore(doc_score.get());
	smeta.SetDocScorer(&ds);
	smeta.SetDocLen(doc_src_length);
    return smeta;
  }

  // destroys forest (replaces it w/ rescored oracle one)
  Oracle ComputeOracle(SentenceMetadata const& smeta,Hypergraph *forest_in_out,WeightVector const& feature_weights,std::ostream *log=0,unsigned kbest=0,std::string const& forest_output="") {
    Hypergraph &forest=*forest_in_out;
    Oracle r;
    int sent_id=smeta.GetSentenceID();
    r.model=Translation(forest);
    if (kbest) DumpKBest("model",sent_id, forest, kbest, true, forest_output);
    {
      Timer t("Forest Oracle rescoring:");
      Hypergraph oracle_forest;
      Rescore(smeta,forest,&oracle_forest,feature_weights,bleu_weight,log);
      forest.swap(oracle_forest);
    }
    r.hope=Translation(forest);
    if (kbest) DumpKBest("oracle",sent_id, forest, kbest, true, forest_output);
    ReweightBleu(&forest,-bleu_weight);
    r.fear=Translation(forest);
    if (kbest) DumpKBest("negative",sent_id, forest, kbest, true, forest_output);
    return r;
  }

  void Rescore(SentenceMetadata const& smeta,Hypergraph const& forest,Hypergraph *dest_forest,WeightVector const& feature_weights,double bleu_weight=1.0,std::ostream *log=&std::cerr) {
    // the sentence bleu stats will get added to doc only if you call IncludeLastScore
    sentscore=GetScore(forest,smeta.GetSentenceID());
	if (!doc_score) { doc_score.reset(sentscore->GetOne()); }
	tmp_src_length = smeta.GetSourceLength(); //TODO: where does this come from?
    using namespace std;
    DenseWeightVector w;
    feature_weights_=feature_weights;
    feature_weights_.set_value(0,bleu_weight);
    feature_weights.init_vector(&w);
    ModelSet oracle_models(w,vector<FeatureFunction const*>(1,pff.get()));
	if (log) *log << "Going to call Apply Model " << endl;
	ApplyModelSet(forest,
                  smeta,
                  oracle_models,
                  IntersectionConfiguration(exhaustive_t()),
                  dest_forest);
    ReweightBleu(dest_forest,bleu_weight);
  }

  void IncludeLastScore(std::ostream *out=0) {
    double bleu_scale_ = doc_src_length * doc_score->ComputeScore();
    doc_score->PlusEquals(*sentscore, scale_oracle);
	sentscore.reset();
    doc_src_length = (doc_src_length + tmp_src_length) * scale_oracle;
    if (out) {
      std::string d;
      doc_score->ScoreDetails(&d);
      *out << "SCALED SCORE: " << bleu_scale_ << "DOC BLEU " << doc_score->ComputeScore() << " " <<d << std::endl;
    }
  }

  void ReweightBleu(Hypergraph *dest_forest,double bleu_weight=-1.) {
    feature_weights_.set_value(0,bleu_weight);
	dest_forest->Reweight(feature_weights_);
//	dest_forest->SortInEdgesByEdgeWeights();
  }

// TODO decoder output should probably be moved to another file - how about oracle_bleu.h
  void DumpKBest(const int sent_id, const Hypergraph& forest, const int k, const bool unique, std::string const &kbest_out_filename_) {
    using namespace std;
    using namespace boost;
    cerr << "In kbest\n";

    ofstream kbest_out;
    kbest_out.open(kbest_out_filename_.c_str());
    cerr << "Output kbest to " << kbest_out_filename_;

    //add length (f side) src length of this sentence to the psuedo-doc src length count
    float curr_src_length = doc_src_length + tmp_src_length;

    if (unique) {
      KBest::KBestDerivations<Sentence, ESentenceTraversal, KBest::FilterUnique> kbest(forest, k);
      for (int i = 0; i < k; ++i) {
        const KBest::KBestDerivations<Sentence, ESentenceTraversal, KBest::FilterUnique>::Derivation* d =
          kbest.LazyKthBest(forest.nodes_.size() - 1, i);
        if (!d) break;
        //calculate score in context of psuedo-doc
        ScoreP sentscore = GetScore(d->yield,sent_id);
        sentscore->PlusEquals(*doc_score,float(1));
        float bleu = curr_src_length * sentscore->ComputeScore();
        kbest_out << sent_id << " ||| " << TD::GetString(d->yield) << " ||| "
                  << d->feature_values << " ||| " << log(d->score) << " ||| " << bleu << endl;
        // cout << sent_id << " ||| " << TD::GetString(d->yield) << " ||| "
        //     << d->feature_values << " ||| " << log(d->score) << endl;
      }
    } else {
      KBest::KBestDerivations<Sentence, ESentenceTraversal> kbest(forest, k);
      for (int i = 0; i < k; ++i) {
        const KBest::KBestDerivations<Sentence, ESentenceTraversal>::Derivation* d =
          kbest.LazyKthBest(forest.nodes_.size() - 1, i);
        if (!d) break;
        cout << sent_id << " ||| " << TD::GetString(d->yield) << " ||| "
             << d->feature_values << " ||| " << log(d->score) << endl;
      }
    }
  }

void DumpKBest(std::string const& suffix,const int sent_id, const Hypergraph& forest, const int k, const bool unique, std::string const& forest_output)
  {
    std::ostringstream kbest_string_stream;
    kbest_string_stream << forest_output << "/kbest_"<<suffix<< "." << sent_id;
    DumpKBest(sent_id, forest, k, unique, kbest_string_stream.str());
  }

};


#endif
