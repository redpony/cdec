#include <gtest/gtest.h>

#include <memory>

#include "mocks/mock_data_array.h"
#include "matchings_sampler.h"
#include "phrase_location.h"

using namespace std;
using namespace ::testing;

namespace extractor {
namespace {

class MatchingsSamplerTest : public Test {
 protected:
  virtual void SetUp() {
    vector<int> locations = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    location = PhraseLocation(locations, 2);

    data_array = make_shared<MockDataArray>();
    for (int i = 0; i < 10; ++i) {
      EXPECT_CALL(*data_array, GetSentenceId(i)).WillRepeatedly(Return(i / 2));
    }
  }

  unordered_set<int> blacklisted_sentence_ids;
  PhraseLocation location;
  shared_ptr<MockDataArray> data_array;
  shared_ptr<MatchingsSampler> sampler;
};

TEST_F(MatchingsSamplerTest, TestSample) {
  sampler = make_shared<MatchingsSampler>(data_array, 1);
  vector<int> expected_locations = {0, 1};
  EXPECT_EQ(PhraseLocation(expected_locations, 2),
            sampler->Sample(location, blacklisted_sentence_ids));

  sampler = make_shared<MatchingsSampler>(data_array, 2);
  expected_locations = {0, 1, 6, 7};
  EXPECT_EQ(PhraseLocation(expected_locations, 2),
            sampler->Sample(location, blacklisted_sentence_ids));

  sampler = make_shared<MatchingsSampler>(data_array, 3);
  expected_locations = {0, 1, 4, 5, 6, 7};
  EXPECT_EQ(PhraseLocation(expected_locations, 2),
            sampler->Sample(location, blacklisted_sentence_ids));

  sampler = make_shared<MatchingsSampler>(data_array, 7);
  expected_locations = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  EXPECT_EQ(PhraseLocation(expected_locations, 2),
            sampler->Sample(location, blacklisted_sentence_ids));
}

TEST_F(MatchingsSamplerTest, TestBackoffSample) {
  sampler = make_shared<MatchingsSampler>(data_array, 1);
  blacklisted_sentence_ids = {0};
  vector<int> expected_locations = {2, 3};
  EXPECT_EQ(PhraseLocation(expected_locations, 2),
            sampler->Sample(location, blacklisted_sentence_ids));

  blacklisted_sentence_ids = {0, 1, 2, 3};
  expected_locations = {8, 9};
  EXPECT_EQ(PhraseLocation(expected_locations, 2),
            sampler->Sample(location, blacklisted_sentence_ids));

  blacklisted_sentence_ids = {0, 1, 2, 3, 4};
  expected_locations = {};
  EXPECT_EQ(PhraseLocation(expected_locations, 2),
            sampler->Sample(location, blacklisted_sentence_ids));

  sampler = make_shared<MatchingsSampler>(data_array, 2);
  blacklisted_sentence_ids = {0, 3};
  expected_locations = {2, 3, 4, 5};
  EXPECT_EQ(PhraseLocation(expected_locations, 2),
            sampler->Sample(location, blacklisted_sentence_ids));

  sampler = make_shared<MatchingsSampler>(data_array, 3);
  blacklisted_sentence_ids = {0, 3};
  expected_locations = {2, 3, 4, 5, 8, 9};
  EXPECT_EQ(PhraseLocation(expected_locations, 2),
            sampler->Sample(location, blacklisted_sentence_ids));

  blacklisted_sentence_ids = {0, 2, 3};
  expected_locations = {2, 3, 8, 9};
  EXPECT_EQ(PhraseLocation(expected_locations, 2),
            sampler->Sample(location, blacklisted_sentence_ids));

  sampler = make_shared<MatchingsSampler>(data_array, 4);
  blacklisted_sentence_ids = {0, 1, 2, 3};
  expected_locations = {8, 9};
  EXPECT_EQ(PhraseLocation(expected_locations, 2),
            sampler->Sample(location, blacklisted_sentence_ids));

  blacklisted_sentence_ids = {1, 3};
  expected_locations = {0, 1, 4, 5, 8, 9};
  EXPECT_EQ(PhraseLocation(expected_locations, 2),
            sampler->Sample(location, blacklisted_sentence_ids));

  sampler = make_shared<MatchingsSampler>(data_array, 7);
  blacklisted_sentence_ids = {0, 1, 2, 3, 4};
  expected_locations = {};
  EXPECT_EQ(PhraseLocation(expected_locations, 2),
            sampler->Sample(location, blacklisted_sentence_ids));

  blacklisted_sentence_ids = {0, 2, 4};
  expected_locations = {2, 3, 6, 7};
  EXPECT_EQ(PhraseLocation(expected_locations, 2),
            sampler->Sample(location, blacklisted_sentence_ids));

  blacklisted_sentence_ids = {1, 3};
  expected_locations = {0, 1, 4, 5, 8, 9};
  EXPECT_EQ(PhraseLocation(expected_locations, 2),
            sampler->Sample(location, blacklisted_sentence_ids));
}

}
} // namespace extractor
