#include <gmock/gmock.h>

#include "../scorer.h"
#include "../features/feature.h"

class MockScorer : public Scorer {
 public:
  MOCK_CONST_METHOD1(Score, vector<double>(const FeatureContext& context));
  MOCK_CONST_METHOD0(GetFeatureNames, vector<string>());
};
