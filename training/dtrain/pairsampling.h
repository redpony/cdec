#ifndef _DTRAIN_PAIRSAMPLING_H_
#define _DTRAIN_PAIRSAMPLING_H_

namespace dtrain
{


bool
accept_pair(score_t a, score_t b, score_t threshold)
{
  if (fabs(a - b) < threshold) return false;
  return true;
}

bool
cmp_hyp_by_score_d(ScoredHyp a, ScoredHyp b)
{
  return a.score > b.score;
}

inline void
all_pairs(vector<ScoredHyp>* s, vector<pair<ScoredHyp,ScoredHyp> >& training, score_t threshold, unsigned max, bool misranked_only, float _unused=1)
{
  sort(s->begin(), s->end(), cmp_hyp_by_score_d);
  unsigned sz = s->size();
  bool b = false;
  unsigned count = 0;
  for (unsigned i = 0; i < sz-1; i++) {
    for (unsigned j = i+1; j < sz; j++) {
      if (misranked_only && !((*s)[i].model <= (*s)[j].model)) continue;
      if (threshold > 0) {
        if (accept_pair((*s)[i].score, (*s)[j].score, threshold))
          training.push_back(make_pair((*s)[i], (*s)[j]));
      } else {
        if ((*s)[i].score != (*s)[j].score)
          training.push_back(make_pair((*s)[i], (*s)[j]));
      }
      if (++count == max) {
        b = true;
        break;
      }
    }
    if (b) break;
  }
}

/*
 * multipartite ranking
 *  sort (descending) by bleu
 *  compare top X to middle Y and low X
 *  cmp middle Y to low X
 */

inline void
partXYX(vector<ScoredHyp>* s, vector<pair<ScoredHyp,ScoredHyp> >& training, score_t threshold, unsigned max, bool misranked_only, float hi_lo)
{
  unsigned sz = s->size();
  if (sz < 2) return;
  sort(s->begin(), s->end(), cmp_hyp_by_score_d);
  unsigned sep = round(sz*hi_lo);
  unsigned sep_hi = sep;
  if (sz > 4) while (sep_hi < sz && (*s)[sep_hi-1].score == (*s)[sep_hi].score) ++sep_hi;
  else sep_hi = 1;
  bool b = false;
  unsigned count = 0;
  for (unsigned i = 0; i < sep_hi; i++) {
    for (unsigned j = sep_hi; j < sz; j++) {
      if (misranked_only && !((*s)[i].model <= (*s)[j].model)) continue;
      if (threshold > 0) {
        if (accept_pair((*s)[i].score, (*s)[j].score, threshold))
          training.push_back(make_pair((*s)[i], (*s)[j]));
      } else {
        if ((*s)[i].score != (*s)[j].score)
          training.push_back(make_pair((*s)[i], (*s)[j]));
      }
      if (++count == max) {
        b = true;
        break;
      }
    }
    if (b) break;
  }
  unsigned sep_lo = sz-sep;
  while (sep_lo > 0 && (*s)[sep_lo-1].score == (*s)[sep_lo].score) --sep_lo;
  for (unsigned i = sep_hi; i < sz-sep_lo; i++) {
    for (unsigned j = sz-sep_lo; j < sz; j++) {
      if (misranked_only && !((*s)[i].model <= (*s)[j].model)) continue;
      if (threshold > 0) {
        if (accept_pair((*s)[i].score, (*s)[j].score, threshold))
          training.push_back(make_pair((*s)[i], (*s)[j]));
      } else {
        if ((*s)[i].score != (*s)[j].score)
          training.push_back(make_pair((*s)[i], (*s)[j]));
      }
      if (++count == max) return;
    }
  }
}

/*
 * pair sampling as in
 * 'Tuning as Ranking' (Hopkins & May, 2011)
 *     count = 5000
 * threshold = 5% BLEU (0.05 for param 3)
 *       cut = top 50
 */
bool
_PRO_cmp_pair_by_diff_d(pair<ScoredHyp,ScoredHyp> a, pair<ScoredHyp,ScoredHyp> b)
{
  return (fabs(a.first.score - a.second.score)) > (fabs(b.first.score - b.second.score));
}
inline void
PROsampling(vector<ScoredHyp>* s, vector<pair<ScoredHyp,ScoredHyp> >& training, score_t threshold, unsigned max, bool _unused=false, float _also_unused=0)
{
  sort(s->begin(), s->end(), cmp_hyp_by_score_d);
  unsigned max_count = 5000, count = 0, sz = s->size();
  bool b = false;
  for (unsigned i = 0; i < sz-1; i++) {
    for (unsigned j = i+1; j < sz; j++) {
      if (accept_pair((*s)[i].score, (*s)[j].score, threshold)) {
        training.push_back(make_pair((*s)[i], (*s)[j]));
        if (++count == max_count) {
          b = true;
          break;
        }
      }
    }
    if (b) break;
  }
  if (training.size() > 50) {
    sort(training.begin(), training.end(), _PRO_cmp_pair_by_diff_d);
    training.erase(training.begin()+50, training.end());
  }
  return;
}


} // namespace

#endif

