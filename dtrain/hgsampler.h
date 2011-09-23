#ifndef _DTRAIN_HGSAMPLER_H_
#define _DTRAIN_HGSAMPLER_H_


#include <vector>
#include "sparse_vector.h"
#include "sampler.h"
#include "wordid.h"

class Hypergraph;

struct HypergraphSampler {

  struct Hypothesis {
    std::vector<WordID> words;
    SparseVector<double> fmap;
    prob_t model_score;
  };

  static void
  sample_hypotheses(const Hypergraph& hg,
                    unsigned n,
                    MT19937* rng,
                    std::vector<Hypothesis>* hypos);
};


#endif

