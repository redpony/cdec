#include "sample_source_count.h"

#include <cmath>

double SampleSourceCount::Score(const FeatureContext& context) const {
  return log10(1 + context.sample_source_count);
}

string SampleSourceCount::GetName() const {
  return "SampleCountF";
}
