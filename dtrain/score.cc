#include "score.h"

namespace dtrain
{


Ngrams
make_ngrams(vector<WordID>& s, size_t N)
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

NgramCounts
make_ngram_counts(vector<WordID> hyp, vector<WordID> ref, size_t N)
{
  Ngrams hyp_ngrams = make_ngrams(hyp, N);
  Ngrams ref_ngrams = make_ngrams(ref, N);
  NgramCounts counts(N);
  Ngrams::iterator it;
  Ngrams::iterator ti;
  for (it = hyp_ngrams.begin(); it != hyp_ngrams.end(); it++) {
    ti = ref_ngrams.find(it->first);
    if (ti != ref_ngrams.end()) {
      counts.add(it->second, ti->second, it->first.size() - 1);
    } else {
      counts.add(it->second, 0, it->first.size() - 1);
    }
  }
  return counts;
}

/*
 * bleu
 *
 * as in "BLEU: a Method for Automatic Evaluation
 *        of Machine Translation"
 * (Papineni et al. '02)
 *
 * NOTE: 0 if one n in {1..N} has 0 count
 */
score_t
brevity_penaly(const size_t hyp_len, const size_t ref_len)
{
  if (hyp_len > ref_len) return 1;
  return exp(1 - (score_t)ref_len/(score_t)hyp_len);
}
score_t
bleu(NgramCounts& counts, const size_t hyp_len, const size_t ref_len,
      size_t N, vector<score_t> weights )
{
  if (hyp_len == 0 || ref_len == 0) return 0;
  if (ref_len < N) N = ref_len;
  score_t N_ = (score_t)N;
  if (weights.empty())
  {
    for (size_t i = 0; i < N; i++) weights.push_back(1/N_);
  }
  score_t sum = 0;
  for (size_t i = 0; i < N; i++) {
    if (counts.clipped[i] == 0 || counts.sum[i] == 0) return 0;
    sum += weights[i] * log((score_t)counts.clipped[i] / (score_t)counts.sum[i]);
  }
  return brevity_penaly(hyp_len, ref_len) * exp(sum);
}

/*
 * 'stupid' bleu
 *
 * as in "ORANGE: a Method for Evaluating
 *        Automatic Evaluation Metrics
 *        for Machine Translation"
 * (Lin & Och '04)
 *
 * NOTE: 0 iff no 1gram match
 */
score_t
stupid_bleu(NgramCounts& counts, const size_t hyp_len, const size_t ref_len,
             size_t N, vector<score_t> weights )
{
  if (hyp_len == 0 || ref_len == 0) return 0;
  if (ref_len < N) N = ref_len;
  score_t N_ = (score_t)N;
  if (weights.empty())
  {
    for (size_t i = 0; i < N; i++) weights.push_back(1/N_);
  }
  score_t sum = 0;
  score_t add = 0;
  for (size_t i = 0; i < N; i++) {
    if (i == 1) add = 1;
    sum += weights[i] * log(((score_t)counts.clipped[i] + add) / ((score_t)counts.sum[i] + add));
  }
  return brevity_penaly(hyp_len, ref_len) * exp(sum);
}

/*
 * smooth bleu
 *
 * as in "An End-to-End Discriminative Approach
 *        to Machine Translation"
 * (Liang et al. '06)
 *
 * NOTE: max is 0.9375
 */
score_t
smooth_bleu(NgramCounts& counts, const size_t hyp_len, const size_t ref_len,
            const size_t N, vector<score_t> weights )
{
  if (hyp_len == 0 || ref_len == 0) return 0;
  score_t N_ = (score_t)N;
  if (weights.empty())
  {
    for (size_t i = 0; i < N; i++) weights.push_back(1/N_);
  }
  score_t sum = 0;
  score_t j = 1;
  for (size_t i = 0; i < N; i++) {
    if (counts.clipped[i] == 0 || counts.sum[i] == 0) continue;
    sum += exp((weights[i] * log((score_t)counts.clipped[i]/(score_t)counts.sum[i]))) / pow(2, N_-j+1);
    j++;
  }
  return brevity_penaly(hyp_len, ref_len) * sum;
}

/*
 * approx. bleu
 *
 * as in "Online Large-Margin Training of Syntactic
 *        and Structural Translation Features"
 * (Chiang et al. '08)
 */
score_t
approx_bleu(NgramCounts& counts, const size_t hyp_len, const size_t ref_len,
            const size_t N, vector<score_t> weights)
{
  return brevity_penaly(hyp_len, ref_len) 
         * 0.9 * bleu(counts, hyp_len, ref_len, N, weights);
}


} // namespace

