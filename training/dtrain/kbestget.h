#ifndef _DTRAIN_KBESTGET_H_
#define _DTRAIN_KBESTGET_H_

#include "kbest.h"

namespace dtrain
{


struct KBestGetter : public HypSampler
{
  const unsigned k_;
  const string filter_type_;
  vector<ScoredHyp> s_;
  unsigned src_len_;

  KBestGetter(const unsigned k, const string filter_type) :
    k_(k), filter_type_(filter_type) {}

  virtual void
  NotifyTranslationForest(const SentenceMetadata& smeta, Hypergraph* hg)
  {
    src_len_ = smeta.GetSourceLength();
    KBestScored(*hg);
  }

  vector<ScoredHyp>* GetSamples() { return &s_; }

  void
  KBestScored(const Hypergraph& forest)
  {
    if (filter_type_ == "uniq") {
      KBestUnique(forest);
    } else if (filter_type_ == "not") {
      KBestNoFilter(forest);
    }
  }

  void
  KBestUnique(const Hypergraph& forest)
  {
    s_.clear(); sz_ = f_count_ = 0;
    KBest::KBestDerivations<vector<WordID>, ESentenceTraversal,
      KBest::FilterUnique, prob_t, EdgeProb> kbest(forest, k_);
    for (unsigned i = 0; i < k_; ++i) {
      const KBest::KBestDerivations<vector<WordID>, ESentenceTraversal, KBest::FilterUnique,
              prob_t, EdgeProb>::Derivation* d =
            kbest.LazyKthBest(forest.nodes_.size() - 1, i);
      if (!d) break;
      ScoredHyp h;
      h.w = d->yield;
      h.f = d->feature_values;
      h.model = log(d->score);
      h.rank = i;
      h.score = scorer_->Score(h.w, *ref_, i, src_len_);
      s_.push_back(h);
      sz_++;
      f_count_ += h.f.size();
    }
  }

  void
  KBestNoFilter(const Hypergraph& forest)
  {
    s_.clear(); sz_ = f_count_ = 0;
    KBest::KBestDerivations<vector<WordID>, ESentenceTraversal> kbest(forest, k_);
    for (unsigned i = 0; i < k_; ++i) {
      const KBest::KBestDerivations<vector<WordID>, ESentenceTraversal>::Derivation* d =
            kbest.LazyKthBest(forest.nodes_.size() - 1, i);
      if (!d) break;
      ScoredHyp h;
      h.w = d->yield;
      h.f = d->feature_values;
      h.model = log(d->score);
      h.rank = i;
      h.score = scorer_->Score(h.w, *ref_, i, src_len_);
      s_.push_back(h);
      sz_++;
      f_count_ += h.f.size();
    }
  }
};


} // namespace

#endif

