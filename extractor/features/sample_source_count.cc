#include "sample_source_count.h"

#include <cmath>

namespace extractor {
namespace features {

double SampleSourceCount::Score(const FeatureContext& context) const {
  return log10(1 + context.num_samples);
}

string SampleSourceCount::GetName() const {
  return "SampleCountF";
}

} // namespace features
} // namespace extractor
