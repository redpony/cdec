#ifndef _DTRAIN_PAIRSAMPLING_H_
#define _DTRAIN_PAIRSAMPLING_H_

namespace dtrain
{


inline void
sample_all_pairs(vector<ScoredHyp>* s, vector<pair<ScoredHyp,ScoredHyp> >& training)
{
  for (unsigned i = 0; i < s->size()-1; i++) {
    for (unsigned j = i+1; j < s->size(); j++) {
      pair<ScoredHyp,ScoredHyp> p;
      p.first = (*s)[i];
      p.second = (*s)[j];
      training.push_back(p);
    }
  }
}

inline void
sample_rand_pairs(vector<ScoredHyp>* s, vector<pair<ScoredHyp,ScoredHyp> >& training,
                  MT19937* prng)
{
  for (unsigned i = 0; i < s->size()-1; i++) {
    for (unsigned j = i+1; j < s->size(); j++) {
      if (prng->next() < .5) {
        pair<ScoredHyp,ScoredHyp> p;
        p.first = (*s)[i];
        p.second = (*s)[j];
        training.push_back(p);
      }
    }
  }
}

bool
sort_samples_by_score(ScoredHyp a, ScoredHyp b)
{
  return a.score < b.score;
}

inline void
sample108010(vector<ScoredHyp>* s, vector<pair<ScoredHyp,ScoredHyp> >& training)
{
  sort(s->begin(), s->end(), sort_samples_by_score);
  pair<ScoredHyp,ScoredHyp>  p;
  unsigned sz = s->size();
  unsigned slice = 10;
  unsigned sep = sz%slice;
  if (sep == 0) sep = sz/slice;
  for (unsigned i = 0; i < sep; i++) {
    for(unsigned j = sep; j < sz; j++) {
      p.first = (*s)[i];
      p.second = (*s)[j];
      if(p.first.rank < p.second.rank) training.push_back(p);
    }
  }
  for (unsigned i = sep; i < sz-sep; i++) {
    for (unsigned j = sz-sep; j < sz; j++) {
      p.first = (*s)[i];
      p.second = (*s)[j];
      if(p.first.rank < p.second.rank) training.push_back(p);
    }
  }
}


} // namespace

#endif

