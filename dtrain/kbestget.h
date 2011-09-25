#ifndef _DTRAIN_KBESTGET_H_
#define _DTRAIN_KBESTGET_H_

#include "kbest.h"

namespace dtrain
{


struct Samples
{
  vector<SparseVector<double> > feats;
  vector<vector<WordID> > sents;
  vector<double> model_scores;
  vector<double> scores;
  size_t GetSize() { return sents.size(); }
};

struct HypoSampler : public DecoderObserver
{
  virtual Samples* GetSamples() {}
};

struct KBestGetter : public HypoSampler
{
  const size_t k_;
  const string filter_type;
  Samples s;

  KBestGetter(const size_t k, const string filter_type) :
    k_(k), filter_type(filter_type) {}

  virtual void
  NotifyTranslationForest(const SentenceMetadata& smeta, Hypergraph* hg)
  {
    KBest(*hg);
  }

  Samples* GetSamples() { return &s; }

  void
  KBest(const Hypergraph& forest)
  {
    if (filter_type == "unique") {
      KBestUnique(forest);
    } else if (filter_type == "no") {
      KBestNoFilter(forest);
    }
  }

  void
  KBestUnique(const Hypergraph& forest)
  {
    s.sents.clear();
    s.feats.clear();
    s.model_scores.clear();
    s.scores.clear();
    KBest::KBestDerivations<vector<WordID>, ESentenceTraversal, KBest::FilterUnique, prob_t, EdgeProb> kbest(forest, k_);
    for (size_t i = 0; i < k_; ++i) {
      const KBest::KBestDerivations<vector<WordID>, ESentenceTraversal, KBest::FilterUnique, prob_t, EdgeProb>::Derivation* d =
            kbest.LazyKthBest(forest.nodes_.size() - 1, i);
      if (!d) break;
      s.sents.push_back(d->yield);
      s.feats.push_back(d->feature_values);
      s.model_scores.push_back(log(d->score));
    }
  }

  void
  KBestNoFilter(const Hypergraph& forest)
  {
    s.sents.clear();
    s.feats.clear();
    s.model_scores.clear();
    s.scores.clear();
    KBest::KBestDerivations<vector<WordID>, ESentenceTraversal> kbest(forest, k_);
    for (size_t i = 0; i < k_; ++i) {
      const KBest::KBestDerivations<vector<WordID>, ESentenceTraversal>::Derivation* d =
            kbest.LazyKthBest(forest.nodes_.size() - 1, i);
      if (!d) break;
      s.sents.push_back(d->yield);
      s.feats.push_back(d->feature_values);
      s.model_scores.push_back(log(d->score));
    }
  }
};


} // namespace

#endif

