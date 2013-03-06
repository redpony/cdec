#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include "mocks/mock_data_array.h"
#include "mocks/mock_suffix_array.h"
#include "precomputation.h"

using namespace std;
using namespace ::testing;

namespace extractor {
namespace {

class PrecomputationTest : public Test {
 protected:
  virtual void SetUp() {
    data = {4, 2, 3, 5, 7, 2, 3, 5, 2, 3, 4, 2, 1};
    data_array = make_shared<MockDataArray>();
    EXPECT_CALL(*data_array, GetData()).WillRepeatedly(ReturnRef(data));

    vector<int> suffixes{12, 8, 5, 1, 9, 6, 2, 0, 10, 7, 3, 4, 13};
    vector<int> lcp{-1, 0, 2, 3, 1, 0, 1, 2, 0, 2, 0, 1, 0, 0};
    suffix_array = make_shared<MockSuffixArray>();
    EXPECT_CALL(*suffix_array, GetData()).WillRepeatedly(Return(data_array));
    for (size_t i = 0; i < suffixes.size(); ++i) {
      EXPECT_CALL(*suffix_array,
                  GetSuffix(i)).WillRepeatedly(Return(suffixes[i]));
    }
    EXPECT_CALL(*suffix_array, BuildLCPArray()).WillRepeatedly(Return(lcp));
  }

  vector<int> data;
  shared_ptr<MockDataArray> data_array;
  shared_ptr<MockSuffixArray> suffix_array;
};

TEST_F(PrecomputationTest, TestCollocations) {
  Precomputation precomputation(suffix_array, 3, 3, 10, 5, 1, 4, 2);
  Index collocations = precomputation.GetCollocations();

  vector<int> key = {2, 3, -1, 2};
  vector<int> expected_value = {1, 5, 1, 8, 5, 8, 5, 11, 8, 11};
  EXPECT_EQ(expected_value, collocations[key]);
  key = {2, 3, -1, 2, 3};
  expected_value = {1, 5, 1, 8, 5, 8};
  EXPECT_EQ(expected_value, collocations[key]);
  key = {2, 3, -1, 3};
  expected_value = {1, 6, 1, 9, 5, 9};
  EXPECT_EQ(expected_value, collocations[key]);
  key = {3, -1, 2};
  expected_value = {2, 5, 2, 8, 2, 11, 6, 8, 6, 11, 9, 11};
  EXPECT_EQ(expected_value, collocations[key]);
  key = {3, -1, 3};
  expected_value = {2, 6, 2, 9, 6, 9};
  EXPECT_EQ(expected_value, collocations[key]);
  key = {3, -1, 2, 3};
  expected_value = {2, 5, 2, 8, 6, 8};
  EXPECT_EQ(expected_value, collocations[key]);
  key = {2, -1, 2};
  expected_value = {1, 5, 1, 8, 5, 8, 5, 11, 8, 11};
  EXPECT_EQ(expected_value, collocations[key]);
  key = {2, -1, 2, 3};
  expected_value = {1, 5, 1, 8, 5, 8};
  EXPECT_EQ(expected_value, collocations[key]);
  key = {2, -1, 3};
  expected_value = {1, 6, 1, 9, 5, 9};
  EXPECT_EQ(expected_value, collocations[key]);

  key = {2, -1, 2, -2, 2};
  expected_value = {1, 5, 8, 5, 8, 11};
  EXPECT_EQ(expected_value, collocations[key]);
  key = {2, -1, 2, -2, 3};
  expected_value = {1, 5, 9};
  EXPECT_EQ(expected_value, collocations[key]);
  key = {2, -1, 3, -2, 2};
  expected_value = {1, 6, 8, 5, 9, 11};
  EXPECT_EQ(expected_value, collocations[key]);
  key = {2, -1, 3, -2, 3};
  expected_value = {1, 6, 9};
  EXPECT_EQ(expected_value, collocations[key]);
  key = {3, -1, 2, -2, 2};
  expected_value = {2, 5, 8, 2, 5, 11, 2, 8, 11, 6, 8, 11};
  EXPECT_EQ(expected_value, collocations[key]);
  key = {3, -1, 2, -2, 3};
  expected_value = {2, 5, 9};
  EXPECT_EQ(expected_value, collocations[key]);
  key = {3, -1, 3, -2, 2};
  expected_value = {2, 6, 8, 2, 6, 11, 2, 9, 11, 6, 9, 11};
  EXPECT_EQ(expected_value, collocations[key]);
  key = {3, -1, 3, -2, 3};
  expected_value = {2, 6, 9};
  EXPECT_EQ(expected_value, collocations[key]);

  // Exceeds max_rule_symbols.
  key = {2, -1, 2, -2, 2, 3};
  EXPECT_EQ(0, collocations.count(key));
  // Contains non frequent pattern.
  key = {2, -1, 5};
  EXPECT_EQ(0, collocations.count(key));
}

} // namespace
} // namespace extractor

