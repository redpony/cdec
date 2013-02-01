#ifndef _IS_SOURCE_SINGLETON_H_
#define _IS_SOURCE_SINGLETON_H_

#include "feature.h"

class IsSourceSingleton : public Feature {
 public:
  double Score(const FeatureContext& context) const;

  string GetName() const;
};

#endif
