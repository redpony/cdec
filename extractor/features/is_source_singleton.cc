#include "is_source_singleton.h"

#include <cmath>

double IsSourceSingleton::Score(const FeatureContext& context) const {
  return context.source_phrase_count == 1;
}

string IsSourceSingleton::GetName() const {
  return "IsSingletonF";
}
