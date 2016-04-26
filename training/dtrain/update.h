#ifndef _DTRAIN_UPDATE_H_
#define _DTRAIN_UPDATE_H_

namespace dtrain
{

/*
 * multipartite [multi=3] ranking
 *  partitions are determined by the 'cut' parameter
 *   0. sort sample (descending) by bleu
 *   1. compare top X(=sz*cut) to middle Y(=sz-2*(sz*cut)) and bottom X
 *      -"- middle Y to bottom X
 *
 */
inline size_t
updates_multipartite(vector<Hyp>* sample,
                     SparseVector<weight_t>& updates,
                     weight_t cut,
                     weight_t margin,
                     size_t max_up,
                     weight_t threshold,
                     bool adjust,
                     bool all,
                     WriteFile& output,
                     size_t id)
{
  size_t up = 0;
  size_t sz = sample->size();
  if (sz < 2) return 0;
  sort(sample->begin(), sample->end(), [](Hyp first, Hyp second)
    {
      return first.gold > second.gold;
    });
  size_t sep = round(sz*cut);

  size_t sep_hi = sep;
  if (adjust) {
    if (sz > 4) {
      while (sep_hi<sz && (*sample)[sep_hi-1].gold==(*sample)[sep_hi].gold)
        ++sep_hi;
    } else {
      sep_hi = 1;
    }
  }
  for (size_t i = 0; i < sep_hi; i++) {
    for (size_t j = sep_hi; j < sz; j++) {
      Hyp& first=(*sample)[i], second=(*sample)[j];
      if (first.gold==second.gold)
        continue;
      if (!all
          && (((first.model-second.model)>margin)
              || (threshold && (first.gold-second.gold < threshold))))
        continue;
      if (output)
        *output << id << "\t" << first.f-second.f << endl;
      updates += first.f-second.f;
      if (++up==max_up)
        return up;
    }
  }

  size_t sep_lo = sz-sep;
  if (adjust) {
    while (sep_lo>0 && (*sample)[sep_lo-1].gold==(*sample)[sep_lo].gold)
        --sep_lo;
  }
  for (size_t i = sep_hi; i < sep_lo; i++) {
    for (size_t j = sep_lo; j < sz; j++) {
      Hyp& first=(*sample)[i], second=(*sample)[j];
      if (first.gold==second.gold)
        continue;
      if (!all
           && (((first.model-second.model)>margin)
               || (threshold && (first.gold-second.gold < threshold))))
        continue;
      if (output)
        *output << id << "\t" << first.f-second.f << endl;
      updates += first.f-second.f;
      if (++up==max_up)
        break;
    }
  }

  return up;
}

/*
 * all pairs
 *  only ignore a pair if gold scores are
 *  identical
 *  FIXME: that's really _all_
 *
 */
inline size_t
updates_all(vector<Hyp>* sample,
            SparseVector<weight_t>& updates,
            size_t max_up,
            weight_t margin,
            weight_t threshold,
            bool all,
            WriteFile output,
            size_t id)
{
  size_t up = 0;
  size_t sz = sample->size();
  sort(sample->begin(), sample->end(), [](Hyp first, Hyp second)
    {
      return first.gold > second.gold;
    });
  for (size_t i = 0; i < sz-1; i++) {
    for (size_t j = i+1; j < sz; j++) {
      Hyp& first=(*sample)[i], second=(*sample)[j];
      if (first.gold == second.gold)
        continue;
      if (!all
          && (((first.model-second.model)>margin)
              || (threshold && (first.gold-second.gold < threshold))))
        continue;
      if (output)
        *output << id << "\t" << first.f-second.f << endl;
      updates += first.f-second.f;
      if (++up==max_up)
        break;
    }
  }

  return up;
}

/*
 * hope/fear
 *  just one pair: hope - fear
 *
 */
inline size_t
update_structured(vector<Hyp>* sample,
                  SparseVector<weight_t>& updates,
                  weight_t margin,
                  WriteFile output,
                  size_t id)
{
  // hope
  sort(sample->begin(), sample->end(), [](Hyp first, Hyp second)
    {
      return (first.model+first.gold) > (second.model+second.gold);
    });
  Hyp hope = (*sample)[0];
  // fear
  sort(sample->begin(), sample->end(), [](Hyp first, Hyp second)
    {
      return (first.model-first.gold) > (second.model-second.gold);
    });
  Hyp fear = (*sample)[0];

  if (hope.gold != fear.gold) {
    updates += hope.f - fear.f;
    if (output)
      *output << id << "\t" << hope.f << "\t" << fear.f << endl;

    return 1;
  }

  if (output)
    *output << endl;

  return 0;
}


/*
 * pair sampling as in
 * 'Tuning as Ranking' (Hopkins & May, 2011)
 *     count = 5000    [maxs]
 * threshold = 5% BLEU [threshold=0.05]
 *       cut = top 50  [max_up]
 */
inline size_t
updates_pro(vector<Hyp>* sample,
           SparseVector<weight_t>& updates,
           size_t maxs,
           size_t max_up,
           weight_t threshold,
           WriteFile& output,
           size_t id)
{

  size_t sz = sample->size(), s;
  vector<pair<Hyp*,Hyp*> > g;
  while (s < maxs) {
    size_t i=rand()%sz, j=rand()%sz;
    Hyp& first=(*sample)[i], second=(*sample)[j];
    if (i==j || fabs(first.gold-second.gold)<threshold)
      continue;
    if (first.gold > second.gold)
      g.emplace_back(make_pair(&first,&second));
    else
      g.emplace_back(make_pair(&second,&first));
    s++;
  }

  if (g.size() > max_up) {
    sort(g.begin(), g.end(), [](pair<Hyp*,Hyp*> a, pair<Hyp*,Hyp*> b)
    {
      return fabs(a.first->gold-a.second->gold)
              > fabs(b.first->gold-b.second->gold);
    });
    g.erase(g.begin()+max_up, g.end());
  }

  for (auto i: g) {
    if (output)
      *output << id << "\t" << i.first->f-i.second->f << endl;
    updates += i.first->f-i.second->f;
  }

  return g.size();
}

/*
 * output (sorted) items in sample (k-best list)
 *
 */
inline void
output_sample(vector<Hyp>* sample,
              WriteFile& output,
              size_t id=0,
              bool sorted=true)
{
  if (sorted) {
    sort(sample->begin(), sample->end(), [](Hyp first, Hyp second)
      {
        return first.gold > second.gold;
      });
  }
  size_t j = 0;
  for (auto k: *sample) {
    *output << id << "\t" << j << "\t" << k.gold << "\t" << k.model
       << "\t" << k.f << endl;
    j++;
  }
}

} // namespace

#endif

