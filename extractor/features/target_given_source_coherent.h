#ifndef _TARGET_GIVEN_SOURCE_COHERENT_H_
#define _TARGET_GIVEN_SOURCE_COHERENT_H_

#include "feature.h"

namespace extractor {
namespace features {

/**
 * Feature computing the ratio of the phrase pair count over all source phrase
 * occurrences (sampled).
 */
class TargetGivenSourceCoherent : public Feature {
 public:
  double Score(const FeatureContext& context) const;

  string GetName() const;
};

} // namespace features
} // namespace extractor

#endif
