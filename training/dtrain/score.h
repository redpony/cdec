#ifndef _DTRAIN_SCORE_H_
#define _DTRAIN_SCORE_H_

#include "dtrain.h"

namespace dtrain
{

struct NgramCounts
{
  size_t N_;
  map<size_t, weight_t> clipped;
  map<size_t, weight_t> sum;

  NgramCounts() {}

  NgramCounts(const size_t N) : N_(N) { zero(); }

  inline void
  operator+=(const NgramCounts& rhs)
  {
    if (rhs.N_ > N_) resize(rhs.N_);
    for (size_t i = 0; i < N_; i++) {
      this->clipped[i] += rhs.clipped.find(i)->second;
      this->sum[i] += rhs.sum.find(i)->second;
    }
  }

  inline void
  operator*=(const weight_t rhs)
  {
    for (size_t i=0; i<N_; i++) {
      this->clipped[i] *= rhs;
      this->sum[i] *= rhs;
    }
  }

  inline void
  add(const size_t count,
      const size_t count_ref,
      const size_t i)
  {
    assert(i < N_);
    if (count > count_ref) {
      clipped[i] += count_ref;
    } else {
      clipped[i] += count;
    }
    sum[i] += count;
  }

  inline void
  zero()
  {
    for (size_t i=0; i<N_; i++) {
      clipped[i] = 0.;
      sum[i] = 0.;
    }
  }

  inline void
  one()
  {
    for (size_t i=0; i<N_; i++) {
      clipped[i] = 1.;
      sum[i] = 1.;
    }
  }

  inline void
  resize(size_t N)
  {
    if (N == N_) return;
    else if (N > N_) {
      for (size_t i = N_; i < N; i++) {
        clipped[i] = 0.;
        sum[i] = 0.;
      }
    } else { // N < N_
      for (size_t i = N_-1; i > N-1; i--) {
        clipped.erase(i);
        sum.erase(i);
      }
    }
    N_ = N;
  }
};

typedef map<vector<WordID>, size_t> Ngrams;

inline Ngrams
ngrams(const vector<WordID>& vw,
       const size_t N)
{
  Ngrams r;
  vector<WordID> ng;
  for (size_t i=0; i<vw.size(); i++) {
    ng.clear();
    for (size_t j=i; j<min(i+N, vw.size()); j++) {
      ng.push_back(vw[j]);
      r[ng]++;
    }
  }

  return r;
}

inline NgramCounts
ngram_counts(const vector<WordID>& hyp,
             const vector<Ngrams>& ngrams_ref,
             const size_t N)
{
  Ngrams ngrams_hyp = ngrams(hyp, N);
  NgramCounts counts(N);
  Ngrams::iterator it, ti;
  for (it = ngrams_hyp.begin(); it != ngrams_hyp.end(); it++) {
    size_t max_ref_count = 0;
    for (auto r: ngrams_ref) {
      ti = r.find(it->first);
      if (ti != r.end())
        max_ref_count = max(max_ref_count, ti->second);
    }
    counts.add(it->second, min(it->second, max_ref_count), it->first.size()-1);
  }

  return counts;
}

class Scorer
{
  protected:
    const size_t     N_;
    vector<weight_t> w_;

  public:
    Scorer(size_t n): N_(n)
    {
      for (size_t i = 1; i <= N_; i++)
        w_.push_back(1.0/N_);
    }

    inline bool
    init(const vector<WordID>& hyp,
         const vector<Ngrams>& reference_ngrams,
         const vector<size_t>& reference_lengths,
         size_t& hl,
         size_t& rl,
         size_t& M,
         vector<weight_t>& v,
         NgramCounts& counts)
    {
      hl = hyp.size();
      if (hl == 0)
        return false;
      rl = best_match_length(hl, reference_lengths);
      if (rl == 0)
        return false;
      counts = ngram_counts(hyp, reference_ngrams, N_);
      if (rl < N_) {
        M = rl;
        for (size_t i = 0; i < M; i++) v.push_back(1/((weight_t)M));
      } else {
        M = N_;
        v = w_;
      }

      return true;
    }

    inline weight_t
    brevity_penalty(const size_t hl,
                    const size_t rl)
    {
      if (hl > rl)
        return 1;

      return exp(1 - (weight_t)rl/hl);
    }

    inline size_t
    best_match_length(const size_t hl,
                      const vector<size_t>& reference_lengths)
    {
      size_t m;
      if (reference_lengths.size() == 1)  {
        m = reference_lengths.front();
      } else {
        size_t i = 0, best_idx = 0;
        size_t best = numeric_limits<size_t>::max();
        for (auto l: reference_lengths) {
          size_t d = abs(hl-l);
          if (d < best) {
            best_idx = i;
            best = d;
          }
          i += 1;
        }
        m = reference_lengths[best_idx];
      }

      return m;
    }

    virtual weight_t
    score(const vector<WordID>&,
          const vector<Ngrams>&,
          const vector<size_t>&) = 0;

    void
    update_context(const vector<WordID>& /*hyp*/,
                   const vector<Ngrams>& /*reference_ngrams*/,
                   const vector<size_t>& /*reference_lengths*/,
                   weight_t /*decay*/) {}
};

/*
 * ['fixed'] per-sentence BLEU
 * simply add 'fix' (1) to reference length for calculation of BP
 * to avoid short translations
 *
 * as in "Optimizing for Sentence-Level BLEU+1
 *        Yields Short Translations"
 * (Nakov et al. '12)
 *
 */
class NakovBleuScorer : public Scorer
{
  weight_t fix;

  public:
    NakovBleuScorer(size_t n, weight_t fix) : Scorer(n), fix(fix) {}

    weight_t
    score(const vector<WordID>& hyp,
          const vector<Ngrams>& reference_ngrams,
          const vector<size_t>& reference_lengths)
    {
      size_t hl, rl, M;
      vector<weight_t> v;
      NgramCounts counts;
      if (!init(hyp, reference_ngrams, reference_lengths, hl, rl, M, v, counts))
        return 0.;
      weight_t sum=0, add=0;
      for (size_t i=0; i<M; i++) {
        if (i == 0 && (counts.sum[i]==0 || counts.clipped[i]==0)) return 0.;
        if (i > 0) add = 1;
        sum += v[i] * log(((weight_t)counts.clipped[i] + add)
                          / ((counts.sum[i] + add)));
      }

      return  brevity_penalty(hl, rl+1) * exp(sum);
    }
};

/*
 * BLEU
 * 0 if for one n \in {1..N} count is 0
 *
 * as in "BLEU: a Method for Automatic Evaluation
 *        of Machine Translation"
 * (Papineni et al. '02)
 *
 */
class PapineniBleuScorer : public Scorer
{
  public:
    PapineniBleuScorer(size_t n) : Scorer(n) {}

    weight_t
    score(const vector<WordID>& hyp,
          const vector<Ngrams>& reference_ngrams,
          const vector<size_t>& reference_lengths)
    {
      size_t hl, rl, M;
      vector<weight_t> v;
      NgramCounts counts;
      if (!init(hyp, reference_ngrams, reference_lengths, hl, rl, M, v, counts))
        return 0.;
      weight_t sum = 0;
      for (size_t i=0; i<M; i++) {
        if (counts.sum[i] == 0 || counts.clipped[i] == 0) return 0.;
        sum += v[i] * log((weight_t)counts.clipped[i]/counts.sum[i]);
      }

      return brevity_penalty(hl, rl) * exp(sum);
    }
};

/*
 * original BLEU+1
 * 0 iff no 1gram match ('grounded')
 *
 * as in "ORANGE: a Method for Evaluating
 *        Automatic Evaluation Metrics
 *        for Machine Translation"
 * (Lin & Och '04)
 *
 */
class LinBleuScorer : public Scorer
{
  public:
    LinBleuScorer(size_t n) : Scorer(n) {}

    weight_t
    score(const vector<WordID>& hyp,
          const vector<Ngrams>& reference_ngrams,
          const vector<size_t>& reference_lengths)
    {
      size_t hl, rl, M;
      vector<weight_t> v;
      NgramCounts counts;
      if (!init(hyp, reference_ngrams, reference_lengths, hl, rl, M, v, counts))
        return 0.;
      weight_t sum=0, add=0;
      for (size_t i=0; i<M; i++) {
        if (i == 0 && (counts.sum[i]==0 || counts.clipped[i]==0)) return 0.;
        if (i == 1) add = 1;
        sum += v[i] * log(((weight_t)counts.clipped[i] + add)
                          / ((counts.sum[i] + add)));
      }

      return  brevity_penalty(hl, rl) * exp(sum);
    }
};

/*
 * smooth BLEU
 * max is 0.9375 (with N=4)
 *
 * as in "An End-to-End Discriminative Approach
 *        to Machine Translation"
 * (Liang et al. '06)
 *
 */
class LiangBleuScorer : public Scorer
{
  public:
    LiangBleuScorer(size_t n) : Scorer(n) {}

    weight_t
    score(const vector<WordID>& hyp,
          const vector<Ngrams>& reference_ngrams,
          const vector<size_t>& reference_lengths)
    {
      size_t hl=hyp.size(), rl=best_match_length(hl, reference_lengths);
      if (hl == 0 || rl == 0) return 0.;
      NgramCounts counts = ngram_counts(hyp, reference_ngrams, N_);
      size_t M = N_;
      if (rl < N_) M = rl;
      weight_t sum = 0.;
      vector<weight_t> i_bleu;
      for (size_t i=0; i<M; i++)
        i_bleu.push_back(0.);
      for (size_t i=0; i<M; i++) {
        if (counts.sum[i]==0 || counts.clipped[i]==0) {
          break;
        } else {
          weight_t i_score = log((weight_t)counts.clipped[i]/counts.sum[i]);
          for (size_t j=i; j < M; j++) {
            i_bleu[j] += (1/((weight_t)j+1)) * i_score;
          }
        }
        sum += exp(i_bleu[i])/pow(2.0, (double)(N_-i+2));
      }

      return brevity_penalty(hl, hl) * sum;
   }
};

/*
 * approx. bleu
 * Needs some more code in dtrain.cc .
 * We do not scale by source length, as hypotheses are compared only
 * within single k-best lists, not globally (as in batch algorithms).
 * TODO: reset after one iteration?
 * TODO: maybe scale by source length?
 *
 * as in "Online Large-Margin Training of Syntactic
 *        and Structural Translation Features"
 * (Chiang et al. '08)
 *
 */
class ChiangBleuScorer : public Scorer
{
  private:
    NgramCounts context;
    weight_t    hyp_sz_sum;
    weight_t    ref_sz_sum;

  public:
    ChiangBleuScorer(size_t n) :
      Scorer(n), context(n), hyp_sz_sum(0), ref_sz_sum(0) {}

    weight_t
    score(const vector<WordID>& hyp,
          const vector<Ngrams>& reference_ngrams,
          const vector<size_t>& reference_lengths)
    {
      size_t hl, rl, M;
      vector<weight_t> v;
      NgramCounts counts;
      if (!init(hyp, reference_ngrams, reference_lengths, hl, rl, M, v, counts))
        return 0.;
      counts += context;
      weight_t sum = 0;
      for (size_t i = 0; i < M; i++) {
        if (counts.sum[i]==0 || counts.clipped[i]==0) return 0.;
        sum += v[i] * log((weight_t)counts.clipped[i] / counts.sum[i]);
      }

      return brevity_penalty(hyp_sz_sum+hl, ref_sz_sum+rl) * exp(sum);
    }

    void
    update_context(const vector<WordID>& hyp,
                   const vector<Ngrams>& reference_ngrams,
                   const vector<size_t>& reference_lengths,
                   weight_t decay=0.9)
    {
      size_t hl, rl, M;
      vector<weight_t> v;
      NgramCounts counts;
      init(hyp, reference_ngrams, reference_lengths, hl, rl, M, v, counts);

      context += counts;
      context *= decay;
      hyp_sz_sum += hl;
      hyp_sz_sum *= decay;
      ref_sz_sum += rl;
      ref_sz_sum *= decay;
    }
};

/*
 * 'sum' bleu
 *
 * Merely sum up Ngram precisions
 */
class SumBleuScorer : public Scorer
{
  public:
    SumBleuScorer(size_t n) : Scorer(n) {}

    weight_t
    score(const vector<WordID>& hyp,
          const vector<Ngrams>& reference_ngrams,
          const vector<size_t>& reference_lengths)
    {
      size_t hl, rl, M;
      vector<weight_t> v;
      NgramCounts counts;
      if (!init(hyp, reference_ngrams, reference_lengths, hl, rl, M, v, counts))
        return 0.;
      weight_t sum = 0.;
      size_t j = 1;
      for (size_t i=0; i<M; i++) {
        if (counts.sum[i]==0 || counts.clipped[i]==0) break;
        sum += ((weight_t)counts.clipped[i]/counts.sum[i])
                / pow(2.0, (weight_t) (N_-j+1));
        //sum += exp(((score_t)counts.clipped[i]/counts.sum[i]))
        //          / pow(2.0, (weight_t) (N_-j+1));
        //sum += exp(v[i] * log(((score_t)counts.clipped[i]/counts.sum[i])))
        //          / pow(2.0, (weight_t) (N_-j+1));
        j++;
      }

      return brevity_penalty(hl, rl) * sum;
    }
};

/*
 * Linear (Corpus) Bleu
 * TODO
 *
 * as in "Lattice Minimum Bayes-Risk Decoding
 *        for Statistical Machine Translation"
 * (Tromble et al. '08)
 * or "Hope and fear for discriminative training of
 *     statistical translation models"
 * (Chiang '12)
 *
 */

} // namespace

#endif

