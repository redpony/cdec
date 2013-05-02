#include "count_source_target.h"

#include <cmath>

namespace extractor {
namespace features {

double CountSourceTarget::Score(const FeatureContext& context) const {
  return log10(1 + context.pair_count);
}

string CountSourceTarget::GetName() const {
  return "CountEF";
}

} // namespace features
} // namespace extractor
