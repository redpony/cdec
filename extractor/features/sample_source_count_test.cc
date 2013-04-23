#include <gtest/gtest.h>

#include <cmath>
#include <memory>
#include <string>

#include "sample_source_count.h"

using namespace std;
using namespace ::testing;

namespace extractor {
namespace features {
namespace {

class SampleSourceCountTest : public Test {
 protected:
  virtual void SetUp() {
    feature = make_shared<SampleSourceCount>();
  }

  shared_ptr<SampleSourceCount> feature;
};

TEST_F(SampleSourceCountTest, TestGetName) {
  EXPECT_EQ("SampleCountF", feature->GetName());
}

TEST_F(SampleSourceCountTest, TestScore) {
  Phrase phrase;
  FeatureContext context(phrase, phrase, 0, 3, 1);
  EXPECT_EQ(log10(2), feature->Score(context));

  context = FeatureContext(phrase, phrase, 3.2, 3, 9);
  EXPECT_EQ(1.0, feature->Score(context));
}

} // namespace
} // namespace features
} // namespace extractor
