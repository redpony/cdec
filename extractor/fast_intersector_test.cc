#include <gtest/gtest.h>

#include <memory>

#include "fast_intersector.h"
#include "mocks/mock_data_array.h"
#include "mocks/mock_precomputation.h"
#include "mocks/mock_suffix_array.h"
#include "mocks/mock_vocabulary.h"
#include "phrase.h"
#include "phrase_location.h"
#include "phrase_builder.h"

using namespace std;
using namespace ::testing;

namespace extractor {
namespace {

class FastIntersectorTest : public Test {
 protected:
  virtual void SetUp() {
    vector<string> words = {"EOL", "it", "makes", "him", "and", "mars", ",",
                            "sets", "on", "takes", "off", "."};
    vocabulary = make_shared<MockVocabulary>();
    for (size_t i = 0; i < words.size(); ++i) {
      EXPECT_CALL(*vocabulary, GetTerminalIndex(words[i]))
          .WillRepeatedly(Return(i));
      EXPECT_CALL(*vocabulary, GetTerminalValue(i))
          .WillRepeatedly(Return(words[i]));
    }

    vector<int> data = {1, 2, 3, 4, 1, 5, 3, 6, 1,
                        7, 3, 8, 4, 1, 9, 3, 10, 11, 0};
    data_array = make_shared<MockDataArray>();
    for (size_t i = 0; i < data.size(); ++i) {
      EXPECT_CALL(*data_array, AtIndex(i)).WillRepeatedly(Return(data[i]));
      EXPECT_CALL(*data_array, GetSentenceId(i))
          .WillRepeatedly(Return(0));
    }
    EXPECT_CALL(*data_array, GetSentenceStart(0))
        .WillRepeatedly(Return(0));
    EXPECT_CALL(*data_array, GetSentenceStart(1))
        .WillRepeatedly(Return(19));
    for (size_t i = 0; i < words.size(); ++i) {
      EXPECT_CALL(*data_array, GetWordId(words[i]))
          .WillRepeatedly(Return(i));
      EXPECT_CALL(*data_array, GetWord(i))
          .WillRepeatedly(Return(words[i]));
    }

    vector<int> suffixes = {18, 0, 4, 8, 13, 1, 2, 6, 10, 15, 3, 12, 5, 7, 9,
                            11, 14, 16, 17};
    suffix_array = make_shared<MockSuffixArray>();
    EXPECT_CALL(*suffix_array, GetData()).WillRepeatedly(Return(data_array));
    for (size_t i = 0; i < suffixes.size(); ++i) {
      EXPECT_CALL(*suffix_array, GetSuffix(i)).
          WillRepeatedly(Return(suffixes[i]));
    }

    precomputation = make_shared<MockPrecomputation>();
    EXPECT_CALL(*precomputation, GetCollocations())
        .WillRepeatedly(ReturnRef(collocations));

    phrase_builder = make_shared<PhraseBuilder>(vocabulary);
    intersector = make_shared<FastIntersector>(suffix_array, precomputation,
                                               vocabulary, 15, 1);
  }

  Index collocations;
  shared_ptr<MockDataArray> data_array;
  shared_ptr<MockSuffixArray> suffix_array;
  shared_ptr<MockPrecomputation> precomputation;
  shared_ptr<MockVocabulary> vocabulary;
  shared_ptr<FastIntersector> intersector;
  shared_ptr<PhraseBuilder> phrase_builder;
};

TEST_F(FastIntersectorTest, TestCachedCollocation) {
  vector<int> symbols = {8, -1, 9};
  vector<int> expected_location = {11};
  Phrase phrase = phrase_builder->Build(symbols);
  PhraseLocation prefix_location(15, 16), suffix_location(16, 17);

  collocations[symbols] = expected_location;
  EXPECT_CALL(*precomputation, GetCollocations())
      .WillRepeatedly(ReturnRef(collocations));
  intersector = make_shared<FastIntersector>(suffix_array, precomputation,
                                             vocabulary, 15, 1);

  PhraseLocation result = intersector->Intersect(
      prefix_location, suffix_location, phrase);

  EXPECT_EQ(PhraseLocation(expected_location, 2), result);
  EXPECT_EQ(PhraseLocation(15, 16), prefix_location);
  EXPECT_EQ(PhraseLocation(16, 17), suffix_location);
}

TEST_F(FastIntersectorTest, TestIntersectaXbXcExtendSuffix) {
  vector<int> symbols = {1, -1, 3, -1, 1};
  Phrase phrase = phrase_builder->Build(symbols);
  vector<int> prefix_locs = {0, 2, 0, 6, 0, 10, 4, 6, 4, 10, 4, 15, 8, 10,
                             8, 15, 3, 15};
  vector<int> suffix_locs = {2, 4, 2, 8, 2, 13, 6, 8, 6, 13, 10, 13};
  PhraseLocation prefix_location(prefix_locs, 2);
  PhraseLocation suffix_location(suffix_locs, 2);

  vector<int> expected_locs = {0, 2, 4, 0, 2, 8, 0, 2, 13, 4, 6, 8, 0, 6, 8,
                               4, 6, 13, 0, 6, 13, 8, 10, 13, 4, 10, 13,
                               0, 10, 13};
  PhraseLocation result = intersector->Intersect(
      prefix_location, suffix_location, phrase);
  EXPECT_EQ(PhraseLocation(expected_locs, 3), result);
}

TEST_F(FastIntersectorTest, TestIntersectaXbExtendPrefix) {
  vector<int> symbols = {1, -1, 3};
  Phrase phrase = phrase_builder->Build(symbols);
  PhraseLocation prefix_location(1, 5), suffix_location(6, 10);

  vector<int> expected_prefix_locs = {0, 4, 8, 13};
  vector<int> expected_locs = {0, 2, 0, 6, 0, 10, 4, 6, 4, 10, 4, 15, 8, 10,
                               8, 15, 13, 15};
  PhraseLocation result = intersector->Intersect(
      prefix_location, suffix_location, phrase);
  EXPECT_EQ(PhraseLocation(expected_locs, 2), result);
  EXPECT_EQ(PhraseLocation(expected_prefix_locs, 1), prefix_location);
}

TEST_F(FastIntersectorTest, TestIntersectCheckEstimates) {
  // The suffix matches in fewer positions, but because it starts with an X
  // it requires more operations and we prefer extending the prefix.
  vector<int> symbols = {1, -1, 4, 1};
  Phrase phrase = phrase_builder->Build(symbols);
  vector<int> prefix_locs = {0, 3, 0, 12, 4, 12, 8, 12};
  PhraseLocation prefix_location(prefix_locs, 2), suffix_location(10, 12);

  vector<int> expected_locs = {0, 3, 0, 12, 4, 12, 8, 12};
  PhraseLocation result = intersector->Intersect(
      prefix_location, suffix_location, phrase);
  EXPECT_EQ(PhraseLocation(expected_locs, 2), result);
  EXPECT_EQ(PhraseLocation(10, 12), suffix_location);
}

} // namespace
} // namespace extractor
