#ifndef _DTRAIN_SAMPLE_H_
#define _DTRAIN_SAMPLE_H_

#include "kbest.h"

#include "score.h"

namespace dtrain
{

struct ScoredKbest : public DecoderObserver
{
  const size_t k_;
  size_t feature_count_, effective_sz_;
  vector<ScoredHyp> samples_;
  Scorer* scorer_;
  vector<Ngrams>* ref_ngs_;
  vector<size_t>* ref_ls_;

  ScoredKbest(const size_t k, Scorer* scorer) :
    k_(k), scorer_(scorer) {}

  virtual void
  NotifyTranslationForest(const SentenceMetadata& /*smeta*/, Hypergraph* hg)
  {
    samples_.clear(); effective_sz_ = feature_count_ = 0;
    KBest::KBestDerivations<vector<WordID>, ESentenceTraversal,
      KBest::FilterUnique, prob_t, EdgeProb> kbest(*hg, k_);
    for (size_t i = 0; i < k_; ++i) {
      const KBest::KBestDerivations<vector<WordID>, ESentenceTraversal,
            KBest::FilterUnique, prob_t, EdgeProb>::Derivation* d =
              kbest.LazyKthBest(hg->nodes_.size() - 1, i);
      if (!d) break;
      ScoredHyp h;
      h.w = d->yield;
      h.f = d->feature_values;
      h.model = log(d->score);
      h.rank = i;
      h.gold = scorer_->Score(h.w, *ref_ngs_, *ref_ls_);
      samples_.push_back(h);
      effective_sz_++;
      feature_count_ += h.f.size();
    }
  }

  vector<ScoredHyp>* GetSamples() { return &samples_; }
  inline void SetReference(vector<Ngrams>& ngs, vector<size_t>& ls)
  {
    ref_ngs_ = &ngs;
    ref_ls_ = &ls;
  }
  inline size_t GetFeatureCount() { return feature_count_; }
  inline size_t GetSize() { return effective_sz_; }
};

} // namespace

#endif

