#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "mocks/mock_feature.h"
#include "scorer.h"

using namespace std;
using namespace ::testing;

namespace extractor {
namespace {

class ScorerTest : public Test {
 protected:
  virtual void SetUp() {
    feature1 = make_shared<features::MockFeature>();
    EXPECT_CALL(*feature1, Score(_)).WillRepeatedly(Return(0.5));
    EXPECT_CALL(*feature1, GetName()).WillRepeatedly(Return("f1"));

    feature2 = make_shared<features::MockFeature>();
    EXPECT_CALL(*feature2, Score(_)).WillRepeatedly(Return(-1.3));
    EXPECT_CALL(*feature2, GetName()).WillRepeatedly(Return("f2"));

    vector<shared_ptr<features::Feature>> features = {feature1, feature2};
    scorer = make_shared<Scorer>(features);
  }

  shared_ptr<features::MockFeature> feature1;
  shared_ptr<features::MockFeature> feature2;
  shared_ptr<Scorer> scorer;
};

TEST_F(ScorerTest, TestScore) {
  vector<double> expected_scores = {0.5, -1.3};
  Phrase phrase;
  features::FeatureContext context(phrase, phrase, 0.3, 2, 11);
  EXPECT_EQ(expected_scores, scorer->Score(context));
}

TEST_F(ScorerTest, TestGetNames) {
  vector<string> expected_names = {"f1", "f2"};
  EXPECT_EQ(expected_names, scorer->GetFeatureNames());
}

} // namespace
} // namespace extractor
