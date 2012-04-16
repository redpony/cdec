#ifndef _RST_H_
#define _RST_H_

#include <vector>
#include "sampler.h"
#include "arc_factored.h"
#include "alias_sampler.h"

struct TreeSampler {
  explicit TreeSampler(const ArcFactoredForest& af);
  void SampleRandomSpanningTree(EdgeSubset* tree, MT19937* rng);
  const ArcFactoredForest& forest;
#define USE_ALIAS_SAMPLER 1
#if USE_ALIAS_SAMPLER
  std::vector<AliasSampler> usucc;
#else
  std::vector<SampleSet<double> > usucc;
#endif
};

#endif
