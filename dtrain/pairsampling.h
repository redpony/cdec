#ifndef _DTRAIN_PAIRSAMPLING_H_
#define _DTRAIN_PAIRSAMPLING_H_

#include "kbestget.h"
#include "sampler.h" // cdec, MT19937

namespace dtrain
{


inline void
sample_all_pairs(vector<ScoredHyp>* s, vector<pair<ScoredHyp,ScoredHyp> > &training)
{
  for (size_t i = 0; i < s->size()-1; i++) {
    for (size_t j = i+1; j < s->size(); j++) {
      pair<ScoredHyp,ScoredHyp> p;
      p.first = (*s)[i];
      p.second = (*s)[j];
      training.push_back(p);
    }
  }
}

inline void
sample_rand_pairs(vector<ScoredHyp>* s, vector<pair<ScoredHyp,ScoredHyp> > &training,
                  MT19937* prng)
{
  for (size_t i = 0; i < s->size()-1; i++) {
    for (size_t j = i+1; j < s->size(); j++) {
      if (prng->next() < .5) {
        pair<ScoredHyp,ScoredHyp> p;
        p.first = (*s)[i];
        p.second = (*s)[j];
        training.push_back(p);
      }
    }
  }
}


} // namespace

#endif

