#include <gtest/gtest.h>

#include <memory>

#include "mocks/mock_suffix_array.h"
#include "mocks/mock_data_array.h"
#include "phrase_location.h"
#include "sampler.h"

using namespace std;
using namespace ::testing;

namespace extractor {
namespace {

class SamplerTest : public Test {
 protected:
  virtual void SetUp() {
    source_data_array = make_shared<MockDataArray>();
    EXPECT_CALL(*source_data_array, GetSentenceId(_)).WillRepeatedly(Return(9999));
    suffix_array = make_shared<MockSuffixArray>();
    EXPECT_CALL(*suffix_array, GetData())
        .WillRepeatedly(Return(source_data_array));
    for (int i = 0; i < 10; ++i) {
      EXPECT_CALL(*suffix_array, GetSuffix(i)).WillRepeatedly(Return(i));
    }
  }

  shared_ptr<MockSuffixArray> suffix_array;
  shared_ptr<Sampler> sampler;
  shared_ptr<MockDataArray> source_data_array;
};

TEST_F(SamplerTest, TestSuffixArrayRange) {
  PhraseLocation location(0, 10);
  unordered_set<int> blacklist;

  sampler = make_shared<Sampler>(suffix_array, 1);
  vector<int> expected_locations = {0};
  EXPECT_EQ(PhraseLocation(expected_locations, 1),
            sampler->Sample(location, blacklist));
  return;

  sampler = make_shared<Sampler>(suffix_array, 2);
  expected_locations = {0, 5};
  EXPECT_EQ(PhraseLocation(expected_locations, 1),
            sampler->Sample(location, blacklist));

  sampler = make_shared<Sampler>(suffix_array, 3);
  expected_locations = {0, 3, 7};
  EXPECT_EQ(PhraseLocation(expected_locations, 1),
            sampler->Sample(location, blacklist));

  sampler = make_shared<Sampler>(suffix_array, 4);
  expected_locations = {0, 3, 5, 8};
  EXPECT_EQ(PhraseLocation(expected_locations, 1),
            sampler->Sample(location, blacklist));

  sampler = make_shared<Sampler>(suffix_array, 100);
  expected_locations = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  EXPECT_EQ(PhraseLocation(expected_locations, 1),
            sampler->Sample(location, blacklist));
}

TEST_F(SamplerTest, TestSubstringsSample) {
  vector<int> locations = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  unordered_set<int> blacklist;
  PhraseLocation location(locations, 2);

  sampler = make_shared<Sampler>(suffix_array, 1);
  vector<int> expected_locations = {0, 1};
  EXPECT_EQ(PhraseLocation(expected_locations, 2),
            sampler->Sample(location, blacklist));

  sampler = make_shared<Sampler>(suffix_array, 2);
  expected_locations = {0, 1, 6, 7};
  EXPECT_EQ(PhraseLocation(expected_locations, 2),
            sampler->Sample(location, blacklist));

  sampler = make_shared<Sampler>(suffix_array, 3);
  expected_locations = {0, 1, 4, 5, 6, 7};
  EXPECT_EQ(PhraseLocation(expected_locations, 2),
            sampler->Sample(location, blacklist));

  sampler = make_shared<Sampler>(suffix_array, 7);
  expected_locations = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  EXPECT_EQ(PhraseLocation(expected_locations, 2),
            sampler->Sample(location, blacklist));
}

} // namespace
} // namespace extractor
