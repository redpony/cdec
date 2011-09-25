#ifndef _DTRAIN_KBESTGET_H_
#define _DTRAIN_KBESTGET_H_

#include "kbest.h"

namespace dtrain
{


struct ScoredHyp
{
  vector<WordID> w;
  SparseVector<double> f;
  score_t model;
  score_t score;
};

struct HypSampler : public DecoderObserver
{
  virtual vector<ScoredHyp>* GetSamples() {}
};

struct KBestGetter : public HypSampler
{
  const unsigned k_;
  const string filter_type_;
  vector<ScoredHyp> s_;

  KBestGetter(const unsigned k, const string filter_type) :
    k_(k), filter_type_(filter_type) {}

  virtual void
  NotifyTranslationForest(const SentenceMetadata& smeta, Hypergraph* hg)
  {
    KBest(*hg);
  }

  vector<ScoredHyp>* GetSamples() { return &s_; }

  void
  KBest(const Hypergraph& forest)
  {
    if (filter_type_ == "unique") {
      KBestUnique(forest);
    } else if (filter_type_ == "no") {
      KBestNoFilter(forest);
    }
  }

  void
  KBestUnique(const Hypergraph& forest)
  {
    s_.clear();
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
      s_.push_back(h);
    }
  }

  void
  KBestNoFilter(const Hypergraph& forest)
  {
    s_.clear();
    KBest::KBestDerivations<vector<WordID>, ESentenceTraversal> kbest(forest, k_);
    for (unsigned i = 0; i < k_; ++i) {
      const KBest::KBestDerivations<vector<WordID>, ESentenceTraversal>::Derivation* d =
            kbest.LazyKthBest(forest.nodes_.size() - 1, i);
      if (!d) break;
      ScoredHyp h;
      h.w = d->yield;
      h.f = d->feature_values;
      h.model = log(d->score);
      s_.push_back(h);
    }
  }
};


} // namespace

#endif

