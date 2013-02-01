#include "is_source_singleton.h"

#include <cmath>

double IsSourceSingleton::Score(const FeatureContext& context) const {
  return context.sample_source_count == 1;
}

string IsSourceSingleton::GetName() const {
  return "IsSingletonF";
}
