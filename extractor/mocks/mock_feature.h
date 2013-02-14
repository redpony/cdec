#include <gmock/gmock.h>

#include "../features/feature.h"

class MockFeature : public Feature {
 public:
  MOCK_CONST_METHOD1(Score, double(const FeatureContext& context));
  MOCK_CONST_METHOD0(GetName, string());
};
