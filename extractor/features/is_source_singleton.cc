#include "is_source_singleton.h"

#include <cmath>

namespace extractor {
namespace features {

double IsSourceSingleton::Score(const FeatureContext& context) const {
  return fabs(context.source_phrase_count - 1) < 1e-6;
}

string IsSourceSingleton::GetName() const {
  return "IsSingletonF";
}

} // namespace features
} // namespace extractor
