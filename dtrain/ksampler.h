#ifndef _DTRAIN_KSAMPLER_H_
#define _DTRAIN_KSAMPLER_H_

#include "hgsampler.h"
#include "kbest.h" // cdec
#include "sampler.h"

namespace dtrain
{


/*
 * KSampler
 *
 */
struct KSampler : public HypoSampler
{
  const size_t k_;
  Samples s;
  MT19937* rng;

  explicit KSampler( const size_t k, MT19937* prng ) :
    k_(k), rng(prng) {}

  virtual void
  NotifyTranslationForest( const SentenceMetadata& smeta, Hypergraph* hg )
  {
    Sample( *hg );
  }

  Samples* GetSamples() { return &s; }

  void Sample( const Hypergraph& forest ) {
    s.sents.clear();
    s.feats.clear();
    s.model_scores.clear();
    s.scores.clear();
    std::vector<HypergraphSampler::Hypothesis> samples;
    HypergraphSampler::sample_hypotheses(forest, k_, rng, &samples);
    for ( size_t i = 0; i < k_; ++i ) {
      s.sents.push_back( samples[i].words );
      s.feats.push_back( samples[i].fmap );
      s.model_scores.push_back( log(samples[i].model_score) );
    }
  }
};


} // namespace

#endif

