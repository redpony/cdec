#ifndef _COUNT_SOURCE_TARGET_H_
#define _COUNT_SOURCE_TARGET_H_

#include "feature.h"

namespace extractor {
namespace features {

class CountSourceTarget : public Feature {
 public:
  double Score(const FeatureContext& context) const;

  string GetName() const;
};

} // namespace features
} // namespace extractor

#endif
