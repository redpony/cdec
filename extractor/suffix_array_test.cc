#include <gtest/gtest.h>

#include "mocks/mock_data_array.h"
#include "phrase_location.h"
#include "suffix_array.h"

#include <vector>

using namespace std;
using namespace ::testing;

namespace extractor {
namespace {

class SuffixArrayTest : public Test {
 protected:
  virtual void SetUp() {
    data = {6, 4, 1, 2, 4, 5, 3, 4, 6, 6, 4, 1, 2};
    data_array = make_shared<MockDataArray>();
    EXPECT_CALL(*data_array, GetData()).WillRepeatedly(ReturnRef(data));
    EXPECT_CALL(*data_array, GetVocabularySize()).WillRepeatedly(Return(7));
    EXPECT_CALL(*data_array, GetSize()).WillRepeatedly(Return(13));
    suffix_array = make_shared<SuffixArray>(data_array);
  }

  vector<int> data;
  shared_ptr<SuffixArray> suffix_array;
  shared_ptr<MockDataArray> data_array;
};

TEST_F(SuffixArrayTest, TestData) {
  EXPECT_EQ(data_array, suffix_array->GetData());
  EXPECT_EQ(14, suffix_array->GetSize());
}

TEST_F(SuffixArrayTest, TestBuildSuffixArray) {
  vector<int> expected_suffix_array =
      {13, 11, 2, 12, 3, 6, 10, 1, 4, 7, 5, 9, 0, 8};
  for (size_t i = 0; i < expected_suffix_array.size(); ++i) {
    EXPECT_EQ(expected_suffix_array[i], suffix_array->GetSuffix(i));
  }
}

TEST_F(SuffixArrayTest, TestBuildLCP) {
  vector<int> expected_lcp = {-1, 0, 2, 0, 1, 0, 0, 3, 1, 1, 0, 0, 4, 1};
  EXPECT_EQ(expected_lcp, suffix_array->BuildLCPArray());
}

TEST_F(SuffixArrayTest, TestLookup) {
  for (size_t i = 0; i < data.size(); ++i) {
    EXPECT_CALL(*data_array, AtIndex(i)).WillRepeatedly(Return(data[i]));
  }

  EXPECT_CALL(*data_array, HasWord("word1")).WillRepeatedly(Return(true));
  EXPECT_CALL(*data_array, GetWordId("word1")).WillRepeatedly(Return(6));
  EXPECT_EQ(PhraseLocation(11, 14), suffix_array->Lookup(0, 14, "word1", 0));

  EXPECT_CALL(*data_array, HasWord("word2")).WillRepeatedly(Return(false));
  EXPECT_EQ(PhraseLocation(0, 0), suffix_array->Lookup(0, 14, "word2", 0));

  EXPECT_CALL(*data_array, HasWord("word3")).WillRepeatedly(Return(true));
  EXPECT_CALL(*data_array, GetWordId("word3")).WillRepeatedly(Return(4));
  EXPECT_EQ(PhraseLocation(11, 13), suffix_array->Lookup(11, 14, "word3", 1));

  EXPECT_CALL(*data_array, HasWord("word4")).WillRepeatedly(Return(true));
  EXPECT_CALL(*data_array, GetWordId("word4")).WillRepeatedly(Return(1));
  EXPECT_EQ(PhraseLocation(11, 13), suffix_array->Lookup(11, 13, "word4", 2));

  EXPECT_CALL(*data_array, HasWord("word5")).WillRepeatedly(Return(true));
  EXPECT_CALL(*data_array, GetWordId("word5")).WillRepeatedly(Return(2));
  EXPECT_EQ(PhraseLocation(11, 13), suffix_array->Lookup(11, 13, "word5", 3));

  EXPECT_EQ(PhraseLocation(12, 13), suffix_array->Lookup(11, 13, "word3", 4));
  EXPECT_EQ(PhraseLocation(11, 11), suffix_array->Lookup(11, 13, "word5", 1));
}

} // namespace
} // namespace extractor
