#ifndef _DTRAIN_SCORE_H_
#define _DTRAIN_SCORE_H_

#include "dtrain.h"

namespace dtrain
{

struct NgramCounts
{
  size_t N_;
  map<size_t, weight_t> clipped_;
  map<size_t, weight_t> sum_;

  NgramCounts() {}

  NgramCounts(const size_t N) : N_(N) { Zero(); }

  inline void
  operator+=(const NgramCounts& rhs)
  {
    if (rhs.N_ > N_) Resize(rhs.N_);
    for (size_t i = 0; i < N_; i++) {
      this->clipped_[i] += rhs.clipped_.find(i)->second;
      this->sum_[i] += rhs.sum_.find(i)->second;
    }
  }

  inline void
  operator*=(const weight_t rhs)
  {
    for (unsigned i = 0; i < N_; i++) {
      this->clipped_[i] *= rhs;
      this->sum_[i] *= rhs;
    }
  }

  inline void
  Add(const size_t count, const size_t ref_count, const size_t i)
  {
    assert(i < N_);
    if (count > ref_count) {
      clipped_[i] += ref_count;
    } else {
      clipped_[i] += count;
    }
    sum_[i] += count;
  }

  inline void
  Zero()
  {
    for (size_t i = 0; i < N_; i++) {
      clipped_[i] = 0.;
      sum_[i] = 0.;
    }
  }

  inline void
  Resize(size_t N)
  {
    if (N == N_) return;
    else if (N > N_) {
      for (size_t i = N_; i < N; i++) {
        clipped_[i] = 0.;
        sum_[i] = 0.;
      }
    } else { // N < N_
      for (size_t i = N_-1; i > N-1; i--) {
        clipped_.erase(i);
        sum_.erase(i);
      }
    }
    N_ = N;
  }
};

typedef map<vector<WordID>, size_t> Ngrams;

inline Ngrams
MakeNgrams(const vector<WordID>& s, const size_t N)
{
  Ngrams ngrams;
  vector<WordID> ng;
  for (size_t i = 0; i < s.size(); i++) {
    ng.clear();
    for (size_t j = i; j < min(i+N, s.size()); j++) {
      ng.push_back(s[j]);
      ngrams[ng]++;
    }
  }

  return ngrams;
}

inline NgramCounts
MakeNgramCounts(const vector<WordID>& hyp,
                const vector<Ngrams>& ref,
                const size_t N)
{
  Ngrams hyp_ngrams = MakeNgrams(hyp, N);
  NgramCounts counts(N);
  Ngrams::iterator it, ti;
  for (it = hyp_ngrams.begin(); it != hyp_ngrams.end(); it++) {
    size_t max_ref_count = 0;
    for (auto r: ref) {
      ti = r.find(it->first);
      if (ti != r.end())
        max_ref_count = max(max_ref_count, ti->second);
    }
    counts.Add(it->second, min(it->second, max_ref_count), it->first.size()-1);
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
    Init(const vector<WordID>& hyp,
         const vector<Ngrams>& ref_ngs,
         const vector<size_t>& ref_ls,
         size_t& hl,
         size_t& rl,
         size_t& M,
         vector<weight_t>& v,
         NgramCounts& counts)
    {
      hl = hyp.size();
      if (hl == 0) return false;
      rl = BestMatchLength(hl, ref_ls);
      if (rl == 0) return false;
      counts = MakeNgramCounts(hyp, ref_ngs, N_);
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
    BrevityPenalty(const size_t hl, const size_t rl)
    {
      if (hl > rl)
        return 1;

      return exp(1 - (weight_t)rl/hl);
    }

    inline size_t
    BestMatchLength(const size_t hl,
                    const vector<size_t>& ref_ls)
    {
      size_t m;
      if (ref_ls.size() == 1)  {
        m = ref_ls.front();
      } else {
        size_t i = 0, best_idx = 0;
        size_t best = numeric_limits<size_t>::max();
        for (auto l: ref_ls) {
          size_t d = abs(hl-l);
          if (d < best) {
            best_idx = i;
            best = d;
          }
          i += 1;
        }
        m = ref_ls[best_idx];
      }

      return m;
    }

    virtual weight_t
    Score(const vector<WordID>&,
          const vector<Ngrams>&,
          const vector<size_t>&) = 0;

    void
    UpdateContext(const vector<WordID>& /*hyp*/,
                  const vector<Ngrams>& /*ref_ngs*/,
                  const vector<size_t>& /*ref_ls*/,
                  weight_t /*decay*/) {}
};

/*
 * 'fixed' per-sentence BLEU
 * simply add 1 to reference length for calculation of BP
 *
 * as in "Optimizing for Sentence-Level BLEU+1
 *        Yields Short Translations"
 * (Nakov et al. '12)
 *
 */
class PerSentenceBleuScorer : public Scorer
{
  public:
    PerSentenceBleuScorer(size_t n) : Scorer(n) {}

    weight_t
    Score(const vector<WordID>& hyp,
          const vector<Ngrams>& ref_ngs,
          const vector<size_t>& ref_ls)
    {
      size_t hl, rl, M;
      vector<weight_t> v;
      NgramCounts counts;
      if (!Init(hyp, ref_ngs, ref_ls, hl, rl, M, v, counts))
        return 0.;
      weight_t sum=0, add=0;
      for (size_t i = 0; i < M; i++) {
        if (i == 0 && (counts.sum_[i] == 0 || counts.clipped_[i] == 0)) return 0.;
        if (i > 0) add = 1;
        sum += v[i] * log(((weight_t)counts.clipped_[i] + add)
                          / ((counts.sum_[i] + add)));
      }

      return  BrevityPenalty(hl, rl+1) * exp(sum);
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

class BleuScorer : public Scorer
{
  public:
    BleuScorer(size_t n) : Scorer(n) {}

    weight_t
    Score(const vector<WordID>& hyp,
          const vector<Ngrams>& ref_ngs,
          const vector<size_t>& ref_ls)
    {
      size_t hl, rl, M;
      vector<weight_t> v;
      NgramCounts counts;
      if (!Init(hyp, ref_ngs, ref_ls, hl, rl, M, v, counts))
        return 0.;
      weight_t sum = 0;
      for (size_t i = 0; i < M; i++) {
        if (counts.sum_[i] == 0 || counts.clipped_[i] == 0) return 0.;
        sum += v[i] * log((weight_t)counts.clipped_[i]/counts.sum_[i]);
      }

      return BrevityPenalty(hl, rl) * exp(sum);
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
class OriginalPerSentenceBleuScorer : public Scorer
{
  public:
    OriginalPerSentenceBleuScorer(size_t n) : Scorer(n) {}

    weight_t
    Score(const vector<WordID>& hyp,
          const vector<Ngrams>& ref_ngs,
          const vector<size_t>& ref_ls)
    {
      size_t hl, rl, M;
      vector<weight_t> v;
      NgramCounts counts;
      if (!Init(hyp, ref_ngs, ref_ls, hl, rl, M, v, counts))
        return 0.;
      weight_t sum=0, add=0;
      for (size_t i = 0; i < M; i++) {
        if (i == 0 && (counts.sum_[i] == 0 || counts.clipped_[i] == 0)) return 0.;
        if (i == 1) add = 1;
        sum += v[i] * log(((weight_t)counts.clipped_[i] + add)/((counts.sum_[i] + add)));
      }

      return  BrevityPenalty(hl, rl) * exp(sum);
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
class SmoothPerSentenceBleuScorer : public Scorer
{
  public:
    SmoothPerSentenceBleuScorer(size_t n) : Scorer(n) {}

    weight_t
    Score(const vector<WordID>& hyp,
          const vector<Ngrams>& ref_ngs,
          const vector<size_t>& ref_ls)
    {
      size_t hl=hyp.size(), rl=BestMatchLength(hl, ref_ls);
      if (hl == 0 || rl == 0) return 0.;
      NgramCounts counts = MakeNgramCounts(hyp, ref_ngs, N_);
      size_t M = N_;
      if (rl < N_) M = rl;
      weight_t sum = 0.;
      vector<weight_t> i_bleu;
      for (size_t i=0; i < M; i++)
        i_bleu.push_back(0.);
      for (size_t i=0; i < M; i++) {
        if (counts.sum_[i] == 0 || counts.clipped_[i] == 0) {
          break;
        } else {
          weight_t i_score = log((weight_t)counts.clipped_[i]/counts.sum_[i]);
          for (size_t j=i; j < M; j++) {
            i_bleu[j] += (1/((weight_t)j+1)) * i_score;
          }
        }
        sum += exp(i_bleu[i])/pow(2.0, (double)(N_-i+2));
      }

      return BrevityPenalty(hl, hl) * sum;
   }
};

/*
 * approx. bleu
 * Needs some more code in dtrain.cc .
 * We do not scaling by source lengths, as hypotheses are compared only
 * within an kbest list, not globally.
 *
 * as in "Online Large-Margin Training of Syntactic
 *        and Structural Translation Features"
 * (Chiang et al. '08)
 *

 */
class ApproxBleuScorer : public Scorer
{
  private:
    NgramCounts context;
    weight_t    hyp_sz_sum;
    weight_t    ref_sz_sum;

  public:
    ApproxBleuScorer(size_t n) :
      Scorer(n), context(n), hyp_sz_sum(0), ref_sz_sum(0) {}

    weight_t
    Score(const vector<WordID>& hyp,
          const vector<Ngrams>& ref_ngs,
          const vector<size_t>& ref_ls)
    {
      size_t hl, rl, M;
      vector<weight_t> v;
      NgramCounts counts;
      if (!Init(hyp, ref_ngs, ref_ls, hl, rl, M, v, counts))
        return 0.;
      counts += context;
      weight_t sum = 0;
      for (size_t i = 0; i < M; i++) {
        if (counts.sum_[i] == 0 || counts.clipped_[i] == 0) return 0.;
        sum += v[i] * log((weight_t)counts.clipped_[i]/counts.sum_[i]);
      }

      return BrevityPenalty(hyp_sz_sum+hl, ref_sz_sum+rl) * exp(sum);
    }

    void
    UpdateContext(const vector<WordID>& hyp,
                  const vector<Ngrams>& ref_ngs,
                  const vector<size_t>& ref_ls,
                  weight_t decay=0.9)
    {
      size_t hl, rl, M;
      vector<weight_t> v;
      NgramCounts counts;
      Init(hyp, ref_ngs, ref_ls, hl, rl, M, v, counts);

      context += counts;
      context *= decay;
      hyp_sz_sum += hl;
      hyp_sz_sum *= decay;
      ref_sz_sum += rl;
      ref_sz_sum *= decay;
    }
};

} // namespace

#endif

