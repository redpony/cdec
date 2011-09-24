#ifndef _DTRAIN_PAIRSAMPLING_H_
#define _DTRAIN_PAIRSAMPLING_H_

#include "kbestget.h"
#include "sampler.h" // cdec, MT19937

namespace dtrain
{


struct TPair
{
  SparseVector<double> first,       second;
  size_t               first_rank,  second_rank;
  double               first_score, second_score;
};

typedef vector<TPair> TrainingInstances;

inline void
sample_all_pairs(KBestList* kb, TrainingInstances &training)
{
  for (size_t i = 0; i < kb->GetSize()-1; i++) {
    for (size_t j = i+1; j < kb->GetSize(); j++) {
      TPair p;
      p.first = kb->feats[i];
      p.second = kb->feats[j];
      p.first_rank = i;
      p.second_rank = j;
      p.first_score = kb->scores[i];
      p.second_score = kb->scores[j];
      training.push_back(p);
    } // j
  } // i
}

inline void
sample_rand_pairs(KBestList* kb, TrainingInstances &training, MT19937* prng)
{
  for (size_t i = 0; i < kb->GetSize()-1; i++) {
    for (size_t j = i+1; j < kb->GetSize(); j++) {
      if (prng->next() < .5) {
        TPair p;
        p.first = kb->feats[i];
        p.second = kb->feats[j];
        p.first_rank = i;
        p.second_rank = j;
        p.first_score = kb->scores[i];
        p.second_score = kb->scores[j];
        training.push_back(p);
      }
    } // j
  } // i
}


} // namespace

#endif

