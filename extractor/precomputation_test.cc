#include <gtest/gtest.h>

#include <memory>
#include <sstream>
#include <vector>

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>

#include "mocks/mock_data_array.h"
#include "mocks/mock_suffix_array.h"
#include "mocks/mock_vocabulary.h"
#include "precomputation.h"

using namespace std;
using namespace ::testing;
namespace ar = boost::archive;

namespace extractor {
namespace {

class PrecomputationTest : public Test {
 protected:
  virtual void SetUp() {
    data = {4, 2, 3, 5, 7, 2, 3, 5, 2, 3, 4, 2, 1};
    data_array = make_shared<MockDataArray>();
    EXPECT_CALL(*data_array, GetData()).WillRepeatedly(Return(data));
    for (size_t i = 0; i < data.size(); ++i) {
      EXPECT_CALL(*data_array, AtIndex(i)).WillRepeatedly(Return(data[i]));
    }
    EXPECT_CALL(*data_array, GetWord(2)).WillRepeatedly(Return("2"));
    EXPECT_CALL(*data_array, GetWord(3)).WillRepeatedly(Return("3"));

    vector<int> suffixes{12, 8, 5, 1, 9, 6, 2, 0, 10, 7, 3, 4, 13};
    vector<int> lcp{-1, 0, 2, 3, 1, 0, 1, 2, 0, 2, 0, 1, 0, 0};
    suffix_array = make_shared<MockSuffixArray>();
    EXPECT_CALL(*suffix_array, GetData()).WillRepeatedly(Return(data_array));
    for (size_t i = 0; i < suffixes.size(); ++i) {
      EXPECT_CALL(*suffix_array,
                  GetSuffix(i)).WillRepeatedly(Return(suffixes[i]));
    }
    EXPECT_CALL(*suffix_array, BuildLCPArray()).WillRepeatedly(Return(lcp));

    vocabulary = make_shared<MockVocabulary>();
    EXPECT_CALL(*vocabulary, GetTerminalIndex("2")).WillRepeatedly(Return(2));
    EXPECT_CALL(*vocabulary, GetTerminalIndex("3")).WillRepeatedly(Return(3));

    precomputation = Precomputation(vocabulary, suffix_array,
                                    3, 3, 10, 5, 1, 4, 2);
  }

  vector<int> data;
  shared_ptr<MockDataArray> data_array;
  shared_ptr<MockSuffixArray> suffix_array;
  shared_ptr<MockVocabulary> vocabulary;
  Precomputation precomputation;
};

TEST_F(PrecomputationTest, TestCollocations) {
  vector<int> key = {2, 3, -1, 2};
  vector<int> expected_value = {1, 5, 1, 8, 5, 8, 5, 11, 8, 11};
  EXPECT_TRUE(precomputation.Contains(key));
  EXPECT_EQ(expected_value, precomputation.GetCollocations(key));
  key = {2, 3, -1, 2, 3};
  expected_value = {1, 5, 1, 8, 5, 8};
  EXPECT_TRUE(precomputation.Contains(key));
  EXPECT_EQ(expected_value, precomputation.GetCollocations(key));
  key = {2, 3, -1, 3};
  expected_value = {1, 6, 1, 9, 5, 9};
  EXPECT_TRUE(precomputation.Contains(key));
  EXPECT_EQ(expected_value, precomputation.GetCollocations(key));
  key = {3, -1, 2};
  expected_value = {2, 5, 2, 8, 2, 11, 6, 8, 6, 11, 9, 11};
  EXPECT_TRUE(precomputation.Contains(key));
  EXPECT_EQ(expected_value, precomputation.GetCollocations(key));
  key = {3, -1, 3};
  expected_value = {2, 6, 2, 9, 6, 9};
  EXPECT_TRUE(precomputation.Contains(key));
  EXPECT_EQ(expected_value, precomputation.GetCollocations(key));
  key = {3, -1, 2, 3};
  expected_value = {2, 5, 2, 8, 6, 8};
  EXPECT_TRUE(precomputation.Contains(key));
  EXPECT_EQ(expected_value, precomputation.GetCollocations(key));
  key = {2, -1, 2};
  expected_value = {1, 5, 1, 8, 5, 8, 5, 11, 8, 11};
  EXPECT_TRUE(precomputation.Contains(key));
  EXPECT_EQ(expected_value, precomputation.GetCollocations(key));
  key = {2, -1, 2, 3};
  expected_value = {1, 5, 1, 8, 5, 8};
  EXPECT_TRUE(precomputation.Contains(key));
  EXPECT_EQ(expected_value, precomputation.GetCollocations(key));
  key = {2, -1, 3};
  expected_value = {1, 6, 1, 9, 5, 9};
  EXPECT_TRUE(precomputation.Contains(key));
  EXPECT_EQ(expected_value, precomputation.GetCollocations(key));

  key = {2, -1, 2, -2, 2};
  expected_value = {1, 5, 8, 5, 8, 11};
  EXPECT_TRUE(precomputation.Contains(key));
  EXPECT_EQ(expected_value, precomputation.GetCollocations(key));
  key = {2, -1, 2, -2, 3};
  expected_value = {1, 5, 9};
  EXPECT_TRUE(precomputation.Contains(key));
  EXPECT_EQ(expected_value, precomputation.GetCollocations(key));
  key = {2, -1, 3, -2, 2};
  expected_value = {1, 6, 8, 5, 9, 11};
  EXPECT_TRUE(precomputation.Contains(key));
  EXPECT_EQ(expected_value, precomputation.GetCollocations(key));
  key = {2, -1, 3, -2, 3};
  expected_value = {1, 6, 9};
  EXPECT_TRUE(precomputation.Contains(key));
  EXPECT_EQ(expected_value, precomputation.GetCollocations(key));
  key = {3, -1, 2, -2, 2};
  expected_value = {2, 5, 8, 2, 5, 11, 2, 8, 11, 6, 8, 11};
  EXPECT_TRUE(precomputation.Contains(key));
  EXPECT_EQ(expected_value, precomputation.GetCollocations(key));
  key = {3, -1, 2, -2, 3};
  expected_value = {2, 5, 9};
  EXPECT_TRUE(precomputation.Contains(key));
  EXPECT_EQ(expected_value, precomputation.GetCollocations(key));
  key = {3, -1, 3, -2, 2};
  expected_value = {2, 6, 8, 2, 6, 11, 2, 9, 11, 6, 9, 11};
  EXPECT_TRUE(precomputation.Contains(key));
  EXPECT_EQ(expected_value, precomputation.GetCollocations(key));
  key = {3, -1, 3, -2, 3};
  expected_value = {2, 6, 9};
  EXPECT_TRUE(precomputation.Contains(key));
  EXPECT_EQ(expected_value, precomputation.GetCollocations(key));

  // Exceeds max_rule_symbols.
  key = {2, -1, 2, -2, 2, 3};
  EXPECT_FALSE(precomputation.Contains(key));
  // Contains non frequent pattern.
  key = {2, -1, 5};
  EXPECT_FALSE(precomputation.Contains(key));
}

TEST_F(PrecomputationTest, TestSerialization) {
  stringstream stream(ios_base::out | ios_base::in);
  ar::text_oarchive output_stream(stream, ar::no_header);
  output_stream << precomputation;

  Precomputation precomputation_copy;
  ar::text_iarchive input_stream(stream, ar::no_header);
  input_stream >> precomputation_copy;

  EXPECT_EQ(precomputation, precomputation_copy);
}

} // namespace
} // namespace extractor

