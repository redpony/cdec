#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include "intersector.h"
#include "mocks/mock_binary_search_merger.h"
#include "mocks/mock_data_array.h"
#include "mocks/mock_linear_merger.h"
#include "mocks/mock_precomputation.h"
#include "mocks/mock_suffix_array.h"
#include "mocks/mock_vocabulary.h"

using namespace std;
using namespace ::testing;

namespace {

class IntersectorTest : public Test {
 protected:
  virtual void SetUp() {
    data = {2, 3, 4, 3, 4, 3};
    vector<string> words = {"a", "b", "c", "b", "c", "b"};
    data_array = make_shared<MockDataArray>();
    EXPECT_CALL(*data_array, GetData()).WillRepeatedly(ReturnRef(data));

    vocabulary = make_shared<MockVocabulary>();
    for (size_t i = 0; i < data.size(); ++i) {
      EXPECT_CALL(*data_array, GetWord(data[i]))
          .WillRepeatedly(Return(words[i]));
      EXPECT_CALL(*vocabulary, GetTerminalIndex(words[i]))
          .WillRepeatedly(Return(data[i]));
      EXPECT_CALL(*vocabulary, GetTerminalValue(data[i]))
          .WillRepeatedly(Return(words[i]));
    }

    vector<int> suffixes = {6, 0, 5, 3, 1, 4, 2};
    suffix_array = make_shared<MockSuffixArray>();
    EXPECT_CALL(*suffix_array, GetData())
        .WillRepeatedly(Return(data_array));
    EXPECT_CALL(*suffix_array, GetSize())
        .WillRepeatedly(Return(suffixes.size()));
    for (size_t i = 0; i < suffixes.size(); ++i) {
      EXPECT_CALL(*suffix_array, GetSuffix(i))
          .WillRepeatedly(Return(suffixes[i]));
    }

    vector<int> key = {2, -1, 4};
    vector<int> values = {0, 2};
    collocations[key] = values;
    precomputation = make_shared<MockPrecomputation>();
    EXPECT_CALL(*precomputation, GetInvertedIndex())
       .WillRepeatedly(ReturnRef(inverted_index));
    EXPECT_CALL(*precomputation, GetCollocations())
       .WillRepeatedly(ReturnRef(collocations));

    linear_merger = make_shared<MockLinearMerger>();
    binary_search_merger = make_shared<MockBinarySearchMerger>();

    phrase_builder = make_shared<PhraseBuilder>(vocabulary);
  }

  Index inverted_index;
  Index collocations;
  vector<int> data;
  shared_ptr<MockVocabulary> vocabulary;
  shared_ptr<MockDataArray> data_array;
  shared_ptr<MockSuffixArray> suffix_array;
  shared_ptr<MockPrecomputation> precomputation;
  shared_ptr<MockLinearMerger> linear_merger;
  shared_ptr<MockBinarySearchMerger> binary_search_merger;
  shared_ptr<PhraseBuilder> phrase_builder;
  shared_ptr<Intersector> intersector;
};

TEST_F(IntersectorTest, TestCachedCollocation) {
  intersector = make_shared<Intersector>(vocabulary, precomputation,
      suffix_array, linear_merger, binary_search_merger, false);

  vector<int> prefix_symbols = {2, -1};
  Phrase prefix = phrase_builder->Build(prefix_symbols);
  vector<int> suffix_symbols = {-1, 4};
  Phrase suffix = phrase_builder->Build(suffix_symbols);
  vector<int> symbols = {2, -1, 4};
  Phrase phrase = phrase_builder->Build(symbols);
  PhraseLocation prefix_locs(0, 1), suffix_locs(2, 3);

  PhraseLocation result = intersector->Intersect(
      prefix, prefix_locs, suffix, suffix_locs, phrase);

  vector<int> expected_locs = {0, 2};
  PhraseLocation expected_result(expected_locs, 2);

  EXPECT_EQ(expected_result, result);
  EXPECT_EQ(PhraseLocation(0, 1), prefix_locs);
  EXPECT_EQ(PhraseLocation(2, 3), suffix_locs);
}

TEST_F(IntersectorTest, TestLinearMergeaXb) {
  vector<int> prefix_symbols = {3, -1};
  Phrase prefix = phrase_builder->Build(prefix_symbols);
  vector<int> suffix_symbols = {-1, 4};
  Phrase suffix = phrase_builder->Build(suffix_symbols);
  vector<int> symbols = {3, -1, 4};
  Phrase phrase = phrase_builder->Build(symbols);
  PhraseLocation prefix_locs(2, 5), suffix_locs(5, 7);

  vector<int> ex_prefix_locs = {1, 3, 5};
  PhraseLocation extended_prefix_locs(ex_prefix_locs, 1);
  vector<int> ex_suffix_locs = {2, 4};
  PhraseLocation extended_suffix_locs(ex_suffix_locs, 1);

  vector<int> expected_locs = {1, 4};
  EXPECT_CALL(*linear_merger, Merge(_, _, _, _, _, _, _, _, _))
      .Times(1)
      .WillOnce(SetArgReferee<0>(expected_locs));
  EXPECT_CALL(*binary_search_merger, Merge(_, _, _, _, _, _, _, _, _)).Times(0);

  intersector = make_shared<Intersector>(vocabulary, precomputation,
      suffix_array, linear_merger, binary_search_merger, false);

  PhraseLocation result = intersector->Intersect(
      prefix, prefix_locs, suffix, suffix_locs, phrase);
  PhraseLocation expected_result(expected_locs, 2);

  EXPECT_EQ(expected_result, result);
  EXPECT_EQ(extended_prefix_locs, prefix_locs);
  EXPECT_EQ(extended_suffix_locs, suffix_locs);
}

TEST_F(IntersectorTest, TestBinarySearchMergeaXb) {
  vector<int> prefix_symbols = {3, -1};
  Phrase prefix = phrase_builder->Build(prefix_symbols);
  vector<int> suffix_symbols = {-1, 4};
  Phrase suffix = phrase_builder->Build(suffix_symbols);
  vector<int> symbols = {3, -1, 4};
  Phrase phrase = phrase_builder->Build(symbols);
  PhraseLocation prefix_locs(2, 5), suffix_locs(5, 7);

  vector<int> ex_prefix_locs = {1, 3, 5};
  PhraseLocation extended_prefix_locs(ex_prefix_locs, 1);
  vector<int> ex_suffix_locs = {2, 4};
  PhraseLocation extended_suffix_locs(ex_suffix_locs, 1);

  vector<int> expected_locs = {1, 4};
  EXPECT_CALL(*binary_search_merger, Merge(_, _, _, _, _, _, _, _, _))
      .Times(1)
      .WillOnce(SetArgReferee<0>(expected_locs));
  EXPECT_CALL(*linear_merger, Merge(_, _, _, _, _, _, _, _, _)).Times(0);

  intersector = make_shared<Intersector>(vocabulary, precomputation,
      suffix_array, linear_merger, binary_search_merger, true);

  PhraseLocation result = intersector->Intersect(
      prefix, prefix_locs, suffix, suffix_locs, phrase);
  PhraseLocation expected_result(expected_locs, 2);

  EXPECT_EQ(expected_result, result);
  EXPECT_EQ(extended_prefix_locs, prefix_locs);
  EXPECT_EQ(extended_suffix_locs, suffix_locs);
}

TEST_F(IntersectorTest, TestMergeaXbXc) {
  vector<int> prefix_symbols = {2, -1, 4, -1};
  Phrase prefix = phrase_builder->Build(prefix_symbols);
  vector<int> suffix_symbols = {-1, 4, -1, 4};
  Phrase suffix = phrase_builder->Build(suffix_symbols);
  vector<int> symbols = {2, -1, 4, -1, 4};
  Phrase phrase = phrase_builder->Build(symbols);

  vector<int> ex_prefix_locs = {0, 2, 0, 4};
  PhraseLocation extended_prefix_locs(ex_prefix_locs, 2);
  vector<int> ex_suffix_locs = {2, 4};
  PhraseLocation extended_suffix_locs(ex_suffix_locs, 2);
  vector<int> expected_locs = {0, 2, 4};
  EXPECT_CALL(*linear_merger, Merge(_, _, _, _, _, _, _, _, _))
      .Times(1)
      .WillOnce(SetArgReferee<0>(expected_locs));
  EXPECT_CALL(*binary_search_merger, Merge(_, _, _, _, _, _, _, _, _)).Times(0);

  intersector = make_shared<Intersector>(vocabulary, precomputation,
      suffix_array, linear_merger, binary_search_merger, false);

  PhraseLocation result = intersector->Intersect(
      prefix, extended_prefix_locs, suffix, extended_suffix_locs, phrase);
  PhraseLocation expected_result(expected_locs, 3);

  EXPECT_EQ(expected_result, result);
  EXPECT_EQ(ex_prefix_locs, *extended_prefix_locs.matchings);
  EXPECT_EQ(ex_suffix_locs, *extended_suffix_locs.matchings);
}

}  // namespace
