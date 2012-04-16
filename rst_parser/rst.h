#ifndef _RST_H_
#define _RST_H_

#include <vector>
#include "sampler.h"
#include "arc_factored.h"

struct TreeSampler {
  explicit TreeSampler(const ArcFactoredForest& af);
  void SampleRandomSpanningTree(EdgeSubset* tree);
  const ArcFactoredForest& forest;
  std::vector<SampleSet<double> > usucc;
};

#endif
