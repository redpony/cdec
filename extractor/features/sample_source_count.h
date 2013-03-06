#ifndef _SAMPLE_SOURCE_COUNT_H_
#define _SAMPLE_SOURCE_COUNT_H_

#include "feature.h"

namespace extractor {
namespace features {

class SampleSourceCount : public Feature {
 public:
  double Score(const FeatureContext& context) const;

  string GetName() const;
};

} // namespace features
} // namespace extractor

#endif
