#include "is_source_target_singleton.h"

#include <cmath>

namespace extractor {
namespace features {

double IsSourceTargetSingleton::Score(const FeatureContext& context) const {
  return context.pair_count == 1;
}

string IsSourceTargetSingleton::GetName() const {
  return "IsSingletonFE";
}

} // namespace features
} // namespace extractor
