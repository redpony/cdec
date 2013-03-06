#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "is_source_target_singleton.h"

using namespace std;
using namespace ::testing;

namespace extractor {
namespace features {
namespace {

class IsSourceTargetSingletonTest : public Test {
 protected:
  virtual void SetUp() {
    feature = make_shared<IsSourceTargetSingleton>();
  }

  shared_ptr<IsSourceTargetSingleton> feature;
};

TEST_F(IsSourceTargetSingletonTest, TestGetName) {
  EXPECT_EQ("IsSingletonFE", feature->GetName());
}

TEST_F(IsSourceTargetSingletonTest, TestScore) {
  Phrase phrase;
  FeatureContext context(phrase, phrase, 0.5, 3, 7);
  EXPECT_EQ(0, feature->Score(context));

  context = FeatureContext(phrase, phrase, 2.3, 1, 28);
  EXPECT_EQ(1, feature->Score(context));
}

} // namespace
} // namespace features
} // namespace extractor
