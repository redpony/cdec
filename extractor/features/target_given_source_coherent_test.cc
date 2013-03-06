#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "target_given_source_coherent.h"

using namespace std;
using namespace ::testing;

namespace extractor {
namespace features {
namespace {

class TargetGivenSourceCoherentTest : public Test {
 protected:
  virtual void SetUp() {
    feature = make_shared<TargetGivenSourceCoherent>();
  }

  shared_ptr<TargetGivenSourceCoherent> feature;
};

TEST_F(TargetGivenSourceCoherentTest, TestGetName) {
  EXPECT_EQ("EgivenFCoherent", feature->GetName());
}

TEST_F(TargetGivenSourceCoherentTest, TestScore) {
  Phrase phrase;
  FeatureContext context(phrase, phrase, 0.3, 2, 20);
  EXPECT_EQ(1.0, feature->Score(context));

  context = FeatureContext(phrase, phrase, 1.9, 0, 1);
  EXPECT_EQ(99.0, feature->Score(context));
}

} // namespace
} // namespace features
} // namespace extractor
