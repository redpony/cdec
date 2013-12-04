#include <gtest/gtest.h>

#include <memory>

#include "mocks/mock_matchings_sampler.h"
#include "mocks/mock_suffix_array_sampler.h"
#include "phrase_location.h"
#include "phrase_location_sampler.h"

using namespace std;
using namespace ::testing;

namespace extractor {
namespace {

class MatchingsSamplerTest : public Test {
 protected:
  virtual void SetUp() {
    matchings_sampler = make_shared<MockMatchingsSampler>();
    suffix_array_sampler = make_shared<MockSuffixArraySampler>();

    sampler = make_shared<PhraseLocationSampler>(
        matchings_sampler, suffix_array_sampler);
  }

  shared_ptr<MockMatchingsSampler> matchings_sampler;
  shared_ptr<MockSuffixArraySampler> suffix_array_sampler;
  shared_ptr<PhraseLocationSampler> sampler;
};

TEST_F(MatchingsSamplerTest, TestSuffixArrayRange) {
  vector<int> locations = {0, 1, 2, 3};
  PhraseLocation location(0, 3), result(locations, 2);
  unordered_set<int> blacklisted_sentence_ids;
  EXPECT_CALL(*suffix_array_sampler, Sample(location, blacklisted_sentence_ids))
      .WillOnce(Return(result));
  EXPECT_EQ(result, sampler->Sample(location, blacklisted_sentence_ids));
}

TEST_F(MatchingsSamplerTest, TestMatchings) {
  vector<int> locations = {0, 1, 2, 3};
  PhraseLocation location(locations, 2), result(locations, 2);
  unordered_set<int> blacklisted_sentence_ids;
  EXPECT_CALL(*matchings_sampler, Sample(location, blacklisted_sentence_ids))
      .WillOnce(Return(result));
  EXPECT_EQ(result, sampler->Sample(location, blacklisted_sentence_ids));
}

}
} // namespace extractor
