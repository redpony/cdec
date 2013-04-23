#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "is_source_singleton.h"

using namespace std;
using namespace ::testing;

namespace extractor {
namespace features {
namespace {

class IsSourceSingletonTest : public Test {
 protected:
  virtual void SetUp() {
    feature = make_shared<IsSourceSingleton>();
  }

  shared_ptr<IsSourceSingleton> feature;
};

TEST_F(IsSourceSingletonTest, TestGetName) {
  EXPECT_EQ("IsSingletonF", feature->GetName());
}

TEST_F(IsSourceSingletonTest, TestScore) {
  Phrase phrase;
  FeatureContext context(phrase, phrase, 0.5, 3, 31);
  EXPECT_EQ(0, feature->Score(context));

  context = FeatureContext(phrase, phrase, 1, 3, 25);
  EXPECT_EQ(1, feature->Score(context));
}

} // namespace
} // namespace features
} // namespace extractor
