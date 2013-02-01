#ifndef _IS_SOURCE_TARGET_SINGLETON_H_
#define _IS_SOURCE_TARGET_SINGLETON_H_

#include "feature.h"

class IsSourceTargetSingleton : public Feature {
 public:
  double Score(const FeatureContext& context) const;

  string GetName() const;
};

#endif
