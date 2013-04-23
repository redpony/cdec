#include <gmock/gmock.h>

#include "features/feature.h"

namespace extractor {
namespace features {

class MockFeature : public Feature {
 public:
  MOCK_CONST_METHOD1(Score, double(const FeatureContext& context));
  MOCK_CONST_METHOD0(GetName, string());
};

} // namespace features
} // namespace extractor
