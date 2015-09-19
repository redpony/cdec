#ifndef _DTRAIN_UPDATE_H_
#define _DTRAIN_UPDATE_H_

namespace dtrain
{

bool
_cmp(ScoredHyp a, ScoredHyp b)
{
  return a.gold > b.gold;
}

bool
_cmpHope(ScoredHyp a, ScoredHyp b)
{
  return (a.model+a.gold) > (b.model+b.gold);
}

bool
_cmpFear(ScoredHyp a, ScoredHyp b)
{
  return (a.model-a.gold) > (b.model-b.gold);
}

inline bool
_good(ScoredHyp& a, ScoredHyp& b, weight_t margin)
{
  if ((a.model-b.model)>margin
      || a.gold==b.gold)
    return true;

  return false;
}

inline bool
_goodS(ScoredHyp& a, ScoredHyp& b)
{
  if (a.gold==b.gold)
    return true;

  return false;
}

/*
 * multipartite ranking
 *  sort (descending) by bleu
 *  compare top X (hi) to middle Y (med) and low X (lo)
 *  cmp middle Y to low X
 */
inline size_t
CollectUpdates(vector<ScoredHyp>* s,
               SparseVector<weight_t>& updates,
               weight_t margin=0.)
{
  size_t num_up = 0;
  size_t sz = s->size();
  sort(s->begin(), s->end(), _cmp);
  size_t sep = round(sz*0.1);
  for (size_t i = 0; i < sep; i++) {
    for (size_t j = sep; j < sz; j++) {
      if (_good((*s)[i], (*s)[j], margin))
        continue;
      updates += (*s)[i].f-(*s)[j].f;
      num_up++;
    }
  }
  size_t sep_lo = sz-sep;
  for (size_t i = sep; i < sep_lo; i++) {
    for (size_t j = sep_lo; j < sz; j++) {
      if (_good((*s)[i], (*s)[j], margin))
        continue;
      updates += (*s)[i].f-(*s)[j].f;
      num_up++;
    }
  }

  return num_up;
}

inline size_t
CollectUpdatesStruct(vector<ScoredHyp>* s,
                     SparseVector<weight_t>& updates,
                     weight_t unused=-1)
{
  // hope
  sort(s->begin(), s->end(), _cmpHope);
  ScoredHyp hope = (*s)[0];
  // fear
  sort(s->begin(), s->end(), _cmpFear);
  ScoredHyp fear = (*s)[0];
  if (!_goodS(hope, fear))
    updates += hope.f - fear.f;

  return updates.size();
}

inline void
OutputKbest(vector<ScoredHyp>* s)
{
  sort(s->begin(), s->end(), _cmp);
  size_t i = 0;
  for (auto k: *s) {
    cout << i << "\t" << k.gold << "\t" << k.model << " \t" << k.f << endl;
    i++;
  }
}

inline void
OutputMultipartitePairs(vector<ScoredHyp>* s,
                               weight_t margin=0.,
                               bool all=true)
{
  size_t sz = s->size();
  sort(s->begin(), s->end(), _cmp);
  size_t sep = round(sz*0.1);
  for (size_t i = 0; i < sep; i++) {
    for (size_t j = sep; j < sz; j++) {
      if (!all && _good((*s)[i], (*s)[j], margin))
        continue;
      cout << (*s)[i].f-(*s)[j].f << endl;
    }
  }
  size_t sep_lo = sz-sep;
  for (size_t i = sep; i < sep_lo; i++) {
    for (size_t j = sep_lo; j < sz; j++) {
      if (!all && _good((*s)[i], (*s)[j], margin))
        continue;
      cout << (*s)[i].f-(*s)[j].f << endl;
    }
  }
}

inline void
OutputAllPairs(vector<ScoredHyp>* s)
{
  size_t sz = s->size();
  sort(s->begin(), s->end(), _cmp);
  for (size_t i = 0; i < sz-1; i++) {
    for (size_t j = i+1; j < sz; j++) {
      if ((*s)[i].gold == (*s)[j].gold)
        continue;
      cout << (*s)[i].f-(*s)[j].f << endl;
    }
  }
}

} // namespace

#endif

