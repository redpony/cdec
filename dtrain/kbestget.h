#ifndef _DTRAIN_KBESTGET_H_
#define _DTRAIN_KBESTGET_H_

#include "kbest.h" // cdec
#include "sentence_metadata.h"

#include "verbose.h"
#include "viterbi.h"
#include "ff_register.h"
#include "decoder.h"
#include "weights.h"
#include "logval.h"

using namespace std;

namespace dtrain
{


typedef double score_t;

struct ScoredHyp
{
  vector<WordID> w;
  SparseVector<double> f;
  score_t model;
  score_t score;
  unsigned rank;
};

struct LocalScorer
{
  unsigned N_;
  vector<score_t> w_;

  virtual score_t
  Score(vector<WordID>& hyp, vector<WordID>& ref, const unsigned rank, const unsigned src_len)=0;

  void Reset() {} // only for approx bleu

  inline void
  Init(unsigned N, vector<score_t> weights)
  {
    assert(N > 0);
    N_ = N;
    if (weights.empty()) for (unsigned i = 0; i < N_; i++) w_.push_back(1./N_);
    else w_ = weights;
  }

  inline score_t
  brevity_penalty(const unsigned hyp_len, const unsigned ref_len)
  {
    if (hyp_len > ref_len) return 1;
    return exp(1 - (score_t)ref_len/hyp_len);
  }
};

struct HypSampler : public DecoderObserver
{
  LocalScorer* scorer_;
  vector<WordID>* ref_;
  unsigned f_count_, sz_;
  virtual vector<ScoredHyp>* GetSamples()=0;
  inline void SetScorer(LocalScorer* scorer) { scorer_ = scorer; }
  inline void SetRef(vector<WordID>& ref) { ref_ = &ref; }
  inline unsigned get_f_count() { return f_count_; }
  inline unsigned get_sz() { return sz_; }
};
////////////////////////////////////////////////////////////////////////////////




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

