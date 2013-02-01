#ifndef _COUNT_SOURCE_TARGET_H_
#define _COUNT_SOURCE_TARGET_H_

#include "feature.h"

class CountSourceTarget : public Feature {
 public:
  double Score(const FeatureContext& context) const;

  string GetName() const;
};

#endif
