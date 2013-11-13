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

class SamplerTestBlacklist : public Test {
 protected:
  virtual void SetUp() {
    source_data_array = make_shared<MockDataArray>();
    for (int i = 0; i < 10; ++i) {
      EXPECT_CALL(*source_data_array, GetSentenceId(i)).WillRepeatedly(Return(i));
    }
    for (int i = -10; i < 0; ++i) {
      EXPECT_CALL(*source_data_array, GetSentenceId(i)).WillRepeatedly(Return(0));
    }
    suffix_array = make_shared<MockSuffixArray>();
    for (int i = -10; i < 10; ++i) {
      EXPECT_CALL(*suffix_array, GetSuffix(i)).WillRepeatedly(Return(i));
    }
  }

  shared_ptr<MockSuffixArray> suffix_array;
  shared_ptr<Sampler> sampler;
  shared_ptr<MockDataArray> source_data_array;
};

TEST_F(SamplerTestBlacklist, TestSuffixArrayRange) {
  PhraseLocation location(0, 10);
  unordered_set<int> blacklist;
  vector<int> expected_locations;
   
  blacklist.insert(0);
  sampler = make_shared<Sampler>(suffix_array, 1);
  expected_locations = {1};
  EXPECT_EQ(PhraseLocation(expected_locations, 1), sampler->Sample(location, blacklist, source_data_array));
  blacklist.clear();
  
  for (int i = 0; i < 9; i++) {
    blacklist.insert(i);
  }
  sampler = make_shared<Sampler>(suffix_array, 1);
  expected_locations = {9};
  EXPECT_EQ(PhraseLocation(expected_locations, 1), sampler->Sample(location, blacklist, source_data_array));
  blacklist.clear();

  blacklist.insert(0);
  blacklist.insert(5);
  sampler = make_shared<Sampler>(suffix_array, 2);
  expected_locations = {1, 4};
  EXPECT_EQ(PhraseLocation(expected_locations, 1), sampler->Sample(location, blacklist, source_data_array));
  blacklist.clear();

  blacklist.insert(0);
  blacklist.insert(1);
  blacklist.insert(2);
  blacklist.insert(3);
  sampler = make_shared<Sampler>(suffix_array, 2);
  expected_locations = {4, 5};
  EXPECT_EQ(PhraseLocation(expected_locations, 1), sampler->Sample(location, blacklist, source_data_array));
  blacklist.clear();

  blacklist.insert(0);
  blacklist.insert(3);
  blacklist.insert(7);
  sampler = make_shared<Sampler>(suffix_array, 3);
  expected_locations = {1, 2, 6};
  EXPECT_EQ(PhraseLocation(expected_locations, 1), sampler->Sample(location, blacklist, source_data_array));
  blacklist.clear();

  blacklist.insert(0);
  blacklist.insert(3);
  blacklist.insert(5);
  blacklist.insert(8);
  sampler = make_shared<Sampler>(suffix_array, 4);
  expected_locations = {1, 2, 4, 7};
  EXPECT_EQ(PhraseLocation(expected_locations, 1), sampler->Sample(location, blacklist, source_data_array));
  blacklist.clear();
  
  blacklist.insert(0);
  sampler = make_shared<Sampler>(suffix_array, 100);
  expected_locations = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  EXPECT_EQ(PhraseLocation(expected_locations, 1), sampler->Sample(location, blacklist, source_data_array));
  blacklist.clear();
 
  blacklist.insert(9);
  sampler = make_shared<Sampler>(suffix_array, 100);
  expected_locations = {0, 1, 2, 3, 4, 5, 6, 7, 8};
  EXPECT_EQ(PhraseLocation(expected_locations, 1), sampler->Sample(location, blacklist, source_data_array));
}

} // namespace
} // namespace extractor
