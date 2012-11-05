#ifndef _HG_SAMPLER_H_
#define _HG_SAMPLER_H_


#include <vector>
#include <string>
#include "sparse_vector.h"
#include "sampler.h"
#include "wordid.h"

class Hypergraph;

struct HypergraphSampler {

  struct Hypothesis {
    std::vector<WordID> words;
    SparseVector<double> fmap;
    prob_t model_score;   // log unnormalized probability
  };

  static void
  sample_hypotheses(const Hypergraph& hg,
                    unsigned n,   // how many samples to draw
                    MT19937* rng,
                    std::vector<Hypothesis>* hypos);

  static void
  sample_trees(const Hypergraph& hg,
               unsigned n,
               MT19937* rng,
               std::vector<std::string>* trees);
};

#endif
