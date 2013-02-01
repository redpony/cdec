#ifndef _TARGET_GIVEN_SOURCE_COHERENT_H_
#define _TARGET_GIVEN_SOURCE_COHERENT_H_

#include "feature.h"

class TargetGivenSourceCoherent : public Feature {
 public:
  double Score(const FeatureContext& context) const;

  string GetName() const;
};

#endif
