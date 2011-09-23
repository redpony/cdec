#ifndef _DTRAIN_KSAMPLER_H_
#define _DTRAIN_KSAMPLER_H_

#include "kbest.h"
#include "hgsampler.h"
#include "sampler.h"

namespace dtrain
{

/*
 * KSampler
 *
 */
struct KSampler : public DecoderObserver
{
  const size_t k_;
  KBestList kb;
  MT19937* rng;

  explicit KSampler( const size_t k, MT19937* prng ) :
    k_(k), rng(prng) {}

  virtual void
  NotifyTranslationForest( const SentenceMetadata& smeta, Hypergraph* hg )
  {
    Sample( *hg );
  }

  KBestList* GetKBest() { return &kb; }

  void Sample( const Hypergraph& forest ) {
    kb.sents.clear();
    kb.feats.clear();
    kb.model_scores.clear();
    kb.scores.clear();
    std::vector<HypergraphSampler::Hypothesis> samples;
    HypergraphSampler::sample_hypotheses(forest, k_, rng, &samples);
    for ( size_t i = 0; i < k_; ++i ) {
      kb.sents.push_back( samples[i].words );
      kb.feats.push_back( samples[i].fmap );
      kb.model_scores.push_back( log(samples[i].model_score) );
    }
  }
};


} // namespace


#endif

