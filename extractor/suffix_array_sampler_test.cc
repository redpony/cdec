#include <gtest/gtest.h>

#include <memory>

#include "mocks/mock_data_array.h"
#include "mocks/mock_suffix_array.h"
#include "suffix_array_sampler.h"

using namespace std;
using namespace ::testing;

namespace extractor {
namespace {

class SuffixArraySamplerTest : public Test {
 protected:
  virtual void SetUp() {
    data_array = make_shared<MockDataArray>();
    for (int i = 0; i < 10; ++i) {
      EXPECT_CALL(*data_array, GetSentenceId(i)).WillRepeatedly(Return(i));
    }

    suffix_array = make_shared<MockSuffixArray>();
    EXPECT_CALL(*suffix_array, GetData()).WillRepeatedly(Return(data_array));
    for (int i = 0; i < 10; ++i) {
      EXPECT_CALL(*suffix_array, GetSuffix(i)).WillRepeatedly(Return(i));
    }
  }

  shared_ptr<MockDataArray> data_array;
  shared_ptr<MockSuffixArray> suffix_array;
};

TEST_F(SuffixArraySamplerTest, TestSample) {
  PhraseLocation location(0, 10);
  unordered_set<int> blacklisted_sentence_ids;

  SuffixArrayRangeSampler sampler(suffix_array, 1);
  vector<int> expected_locations = {0};
  EXPECT_EQ(PhraseLocation(expected_locations, 1),
            sampler.Sample(location, blacklisted_sentence_ids));

  sampler = SuffixArrayRangeSampler(suffix_array, 2);
  expected_locations = {0, 5};
  EXPECT_EQ(PhraseLocation(expected_locations, 1),
            sampler.Sample(location, blacklisted_sentence_ids));

  sampler = SuffixArrayRangeSampler(suffix_array, 3);
  expected_locations = {0, 3, 7};
  EXPECT_EQ(PhraseLocation(expected_locations, 1),
            sampler.Sample(location, blacklisted_sentence_ids));

  sampler = SuffixArrayRangeSampler(suffix_array, 4);
  expected_locations = {0, 3, 5, 8};
  EXPECT_EQ(PhraseLocation(expected_locations, 1),
            sampler.Sample(location, blacklisted_sentence_ids));

  sampler = SuffixArrayRangeSampler(suffix_array, 100);
  expected_locations = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  EXPECT_EQ(PhraseLocation(expected_locations, 1),
            sampler.Sample(location, blacklisted_sentence_ids));
}

TEST_F(SuffixArraySamplerTest, TestBackoffSample) {
  PhraseLocation location(0, 10);

  SuffixArrayRangeSampler sampler(suffix_array, 1);
  unordered_set<int> blacklisted_sentence_ids = {0};
  vector<int> expected_locations = {1};
  EXPECT_EQ(PhraseLocation(expected_locations, 1),
            sampler.Sample(location, blacklisted_sentence_ids));

  blacklisted_sentence_ids = {0, 1, 2, 3, 4, 5, 6, 7, 8};
  expected_locations = {9};
  EXPECT_EQ(PhraseLocation(expected_locations, 1),
            sampler.Sample(location, blacklisted_sentence_ids));

  sampler = SuffixArrayRangeSampler(suffix_array, 2);
  blacklisted_sentence_ids = {0, 5};
  expected_locations = {1, 4};
  EXPECT_EQ(PhraseLocation(expected_locations, 1),
            sampler.Sample(location, blacklisted_sentence_ids));

  blacklisted_sentence_ids = {0, 1, 2, 3};
  expected_locations = {4, 5};
  EXPECT_EQ(PhraseLocation(expected_locations, 1),
            sampler.Sample(location, blacklisted_sentence_ids));

  sampler = SuffixArrayRangeSampler(suffix_array, 3);
  blacklisted_sentence_ids = {0, 3, 7};
  expected_locations = {1, 2, 6};
  EXPECT_EQ(PhraseLocation(expected_locations, 1),
            sampler.Sample(location, blacklisted_sentence_ids));

  sampler = SuffixArrayRangeSampler(suffix_array, 4);
  blacklisted_sentence_ids = {0, 3, 5, 8};
  expected_locations = {1, 2, 4, 7};
  EXPECT_EQ(PhraseLocation(expected_locations, 1),
            sampler.Sample(location, blacklisted_sentence_ids));

  sampler = SuffixArrayRangeSampler(suffix_array, 100);
  blacklisted_sentence_ids = {0};
  expected_locations = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  EXPECT_EQ(PhraseLocation(expected_locations, 1),
            sampler.Sample(location, blacklisted_sentence_ids));

  blacklisted_sentence_ids = {9};
  expected_locations = {0, 1, 2, 3, 4, 5, 6, 7, 8};
  EXPECT_EQ(PhraseLocation(expected_locations, 1),
            sampler.Sample(location, blacklisted_sentence_ids));
}

}
} // namespace extractor
