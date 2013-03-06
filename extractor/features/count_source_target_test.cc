#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "count_source_target.h"

using namespace std;
using namespace ::testing;

namespace extractor {
namespace features {
namespace {

class CountSourceTargetTest : public Test {
 protected:
  virtual void SetUp() {
    feature = make_shared<CountSourceTarget>();
  }

  shared_ptr<CountSourceTarget> feature;
};

TEST_F(CountSourceTargetTest, TestGetName) {
  EXPECT_EQ("CountEF", feature->GetName());
}

TEST_F(CountSourceTargetTest, TestScore) {
  Phrase phrase;
  FeatureContext context(phrase, phrase, 0.5, 9, 13);
  EXPECT_EQ(1.0, feature->Score(context));
}

} // namespace
} // namespace features
} // namespace extractor
