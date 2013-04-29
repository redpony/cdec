#ifndef _DTRAIN_KSAMPLER_H_
#define _DTRAIN_KSAMPLER_H_

#include "hg_sampler.h"

namespace dtrain
{


bool
cmp_hyp_by_model_d(ScoredHyp a, ScoredHyp b)
{
  return a.model > b.model;
}

struct KSampler : public HypSampler
{
  const unsigned k_;
  vector<ScoredHyp> s_;
  MT19937* prng_;
  score_t (*scorer)(NgramCounts&, const unsigned, const unsigned, unsigned, vector<score_t>);
  unsigned src_len_;

  explicit KSampler(const unsigned k, MT19937* prng) :
    k_(k), prng_(prng) {}

  virtual void
  NotifyTranslationForest(const SentenceMetadata& smeta, Hypergraph* hg)
  {
    src_len_ = smeta.GetSourceLength();
    ScoredSamples(*hg);
  }

  vector<ScoredHyp>* GetSamples() { return &s_; }

  void ScoredSamples(const Hypergraph& forest) {
    s_.clear(); sz_ = f_count_ = 0;
    std::vector<HypergraphSampler::Hypothesis> samples;
    HypergraphSampler::sample_hypotheses(forest, k_, prng_, &samples);
    for (unsigned i = 0; i < k_; ++i) {
      ScoredHyp h;
      h.w = samples[i].words;
      h.f = samples[i].fmap;
      h.model = log(samples[i].model_score);
      h.rank = i;
      h.score = scorer_->Score(h.w, *ref_, i, src_len_);
      s_.push_back(h);
      sz_++;
      f_count_ += h.f.size();
    }
    sort(s_.begin(), s_.end(), cmp_hyp_by_model_d);
    for (unsigned i = 0; i < s_.size(); i++) s_[i].rank = i;
  }
};


} // namespace

#endif

