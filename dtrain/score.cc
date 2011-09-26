#include "score.h"

namespace dtrain
{


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
BleuScorer::Bleu(NgramCounts& counts, const unsigned hyp_len, const unsigned ref_len)
{
  if (hyp_len == 0 || ref_len == 0) return 0;
  unsigned M = N_;
  if (ref_len < N_) M = ref_len;
  score_t sum = 0;
  for (unsigned i = 0; i < M; i++) {
    if (counts.clipped[i] == 0 || counts.sum[i] == 0) return 0;
    sum += w_[i] * log((score_t)counts.clipped[i]/counts.sum[i]);
  }
  return brevity_penaly(hyp_len, ref_len) * exp(sum);
}

score_t
BleuScorer::Score(vector<WordID>& hyp, vector<WordID>& ref)
{
  unsigned hyp_len = hyp.size(), ref_len = ref.size();
  if (hyp_len == 0 || ref_len == 0) return 0;
  NgramCounts counts = make_ngram_counts(hyp, ref, N_);
  return Bleu(counts, hyp_len, ref_len);
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
StupidBleuScorer::Score(vector<WordID>& hyp, vector<WordID>& ref)
{
  unsigned hyp_len = hyp.size(), ref_len = ref.size();
  if (hyp_len == 0 || ref_len == 0) return 0;
  NgramCounts counts = make_ngram_counts(hyp, ref, N_);
  unsigned M = N_;
  if (ref_len < N_) M = ref_len;
  score_t sum = 0, add = 0;
  for (unsigned i = 0; i < M; i++) {
    if (i == 1) add = 1;
    sum += w_[i] * log(((score_t)counts.clipped[i] + add)/((counts.sum[i] + add)));
  }
  return  brevity_penaly(hyp_len, ref_len) * exp(sum);
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
SmoothBleuScorer::Score(vector<WordID>& hyp, vector<WordID>& ref)
{
  unsigned hyp_len = hyp.size(), ref_len = ref.size();
  if (hyp_len == 0 || ref_len == 0) return 0;
  NgramCounts counts = make_ngram_counts(hyp, ref, N_);
  score_t sum = 0;
  unsigned j = 1;
  for (unsigned i = 0; i < N_; i++) {
    if (counts.clipped[i] == 0 || counts.sum[i] == 0) continue;
    sum += exp((w_[i] * log((score_t)counts.clipped[i]/counts.sum[i])))/pow(2, N_-j+1);
    j++;
  }
  return brevity_penaly(hyp_len, ref_len) * sum;
}

// FIXME
/*
 * approx. bleu
 *
 * as in "Online Large-Margin Training of Syntactic
 *        and Structural Translation Features"
 * (Chiang et al. '08)
 */
/*void
ApproxBleuScorer::Prep(NgramCounts& counts, const unsigned hyp_len, const unsigned ref_len)
{
  glob_onebest_counts += counts;
  glob_hyp_len += hyp_len;
  glob_ref_len += ref_len;
}

void
ApproxBleuScorer::Reset()
{
  glob_onebest_counts.Zero();
  glob_hyp_len = 0;
  glob_ref_len = 0;
}

score_t
ApproxBleuScorer::Score(ScoredHyp& hyp, vector<WordID>& ref_ids, unsigned id)
{
  NgramCounts counts = make_ngram_counts(hyp.w, ref_ids, N_);
  if (id == 0) reset();
  unsigned hyp_len = 0, ref_len = 0;
  if (hyp.rank == 0) { // 'context of 1best translations'
    scorer->prep(counts, hyp.w.size(), ref_ids.size()); 
    counts.reset();
  } else {
    hyp_len = hyp.w.size();
    ref_len = ref_ids.size();
  }
  return 0.9 * BleuScorer::Bleu(glob_onebest_counts + counts,
                                glob_hyp_len + hyp_len, glob_ref_len + ref_len);
}*/


} // namespace

