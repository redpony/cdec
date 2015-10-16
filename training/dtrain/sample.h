#ifndef _DTRAIN_SAMPLE_H_
#define _DTRAIN_SAMPLE_H_

#include "kbest.h"
#include "hg_sampler.h"

#include "score.h"

namespace dtrain
{

struct HypSampler : public DecoderObserver
{
  size_t          feature_count, effective_size;
  vector<Hyp>     sample;
  vector<Ngrams>* reference_ngrams;
  vector<size_t>* reference_lengths;

  void
  reset()
  {
    sample.clear();
    effective_size = feature_count = 0;
  }
};

struct KBestSampler : public HypSampler
{
  size_t  k;
  bool    unique;
  Scorer* scorer;

  KBestSampler() {}
  KBestSampler(const size_t k, Scorer* scorer) :
    k(k), scorer(scorer) {}

  virtual void
  NotifyTranslationForest(const SentenceMetadata& /*smeta*/, Hypergraph* hg)
  {
    reset();
    KBest::KBestDerivations<vector<WordID>, ESentenceTraversal,
      KBest::FilterUnique, prob_t, EdgeProb> kbest(*hg, k);
    for (size_t i=0; i<k; ++i) {
      KBest::KBestDerivations<vector<WordID>, ESentenceTraversal,
        KBest::FilterUnique, prob_t, EdgeProb>::Derivation* d =
          kbest.LazyKthBest(hg->nodes_.size() - 1, i);
      if (!d) break;
      sample.emplace_back(
        d->yield,
        d->feature_values,
        log(d->score),
        scorer->score(d->yield, *reference_ngrams, *reference_lengths),
        i
      );
      effective_size++;
      feature_count += sample.back().f.size();
    }
  }
};

struct KBestNoFilterSampler : public KBestSampler
{
  size_t  k;
  bool    unique;
  Scorer* scorer;

  KBestNoFilterSampler(const size_t k, Scorer* scorer) :
    k(k), scorer(scorer) {}

  virtual void
  NotifyTranslationForest(const SentenceMetadata& /*smeta*/, Hypergraph* hg)
  {
    reset();
    KBest::KBestDerivations<vector<WordID>, ESentenceTraversal> kbest(*hg, k);
    for (size_t i=0; i<k; ++i) {
      const KBest::KBestDerivations<vector<WordID>, ESentenceTraversal>::Derivation* d =
        kbest.LazyKthBest(hg->nodes_.size() - 1, i);
      if (!d) break;
      sample.emplace_back(
        d->yield,
        d->feature_values,
        log(d->score),
        scorer->score(d->yield, *reference_ngrams, *reference_lengths),
        i
      );
      effective_size++;
      feature_count += sample.back().f.size();
    }
  }
};

struct KSampler : public HypSampler
{
  const size_t k;
  Scorer* scorer;
  MT19937 rng;

  explicit KSampler(const unsigned k, Scorer* scorer) :
    k(k), scorer(scorer) {}

  virtual void
  NotifyTranslationForest(const SentenceMetadata& /*smeta*/, Hypergraph* hg)
  {
    reset();
    std::vector<HypergraphSampler::Hypothesis> hs;
    HypergraphSampler::sample_hypotheses(*hg, k, &rng, &hs);
    for (size_t i=0; i<k; ++i) {
      sample.emplace_back(
        hs[i].words,
        hs[i].fmap,
        log(hs[i].model_score),
        0,
        0
      );
      effective_size++;
      feature_count += sample.back().f.size();
    }
    sort(sample.begin(), sample.end(), [](Hyp first, Hyp second) {
        return first.model > second.model;
      });
    for (unsigned i=0; i<sample.size(); i++) {
      sample[i].rank=i;
      scorer->score(sample[i].w, *reference_ngrams, *reference_lengths);
    }
  }
};

} // namespace

#endif

