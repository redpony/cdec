#include <gtest/gtest.h>

#include <memory>

#include "binary_search_merger.h"
#include "matching_comparator.h"
#include "mocks/mock_data_array.h"
#include "mocks/mock_vocabulary.h"
#include "mocks/mock_linear_merger.h"
#include "phrase.h"
#include "phrase_location.h"
#include "phrase_builder.h"

using namespace std;
using namespace ::testing;

namespace {

class BinarySearchMergerTest : public Test {
 protected:
  virtual void SetUp() {
    shared_ptr<MockVocabulary> vocabulary = make_shared<MockVocabulary>();
    EXPECT_CALL(*vocabulary, GetTerminalValue(_))
        .WillRepeatedly(Return("word"));

    shared_ptr<MockDataArray> data_array = make_shared<MockDataArray>();
    EXPECT_CALL(*data_array, GetSentenceId(_))
        .WillRepeatedly(Return(1));

    shared_ptr<MatchingComparator> comparator =
        make_shared<MatchingComparator>(1, 20);

    phrase_builder = make_shared<PhraseBuilder>(vocabulary);

    // We are going to force the binary_search_merger to do all the work, so we
    // need to check that the linear_merger never gets called.
    shared_ptr<MockLinearMerger> linear_merger = make_shared<MockLinearMerger>(
        vocabulary, data_array, comparator);
    EXPECT_CALL(*linear_merger, Merge(_, _, _, _, _, _, _, _, _)).Times(0);

    binary_search_merger = make_shared<BinarySearchMerger>(
        vocabulary, linear_merger, data_array, comparator, true);
  }

  shared_ptr<BinarySearchMerger> binary_search_merger;
  shared_ptr<PhraseBuilder> phrase_builder;
};

TEST_F(BinarySearchMergerTest, aXbTest) {
  vector<int> locations;
  // Encoding for him X it (see Adam's dissertation).
  vector<int> symbols{1, -1, 2};
  Phrase phrase = phrase_builder->Build(symbols);
  vector<int> suffix_symbols{-1, 2};
  Phrase suffix = phrase_builder->Build(suffix_symbols);

  vector<int> prefix_locs{2, 6, 10, 15};
  vector<int> suffix_locs{0, 4, 8, 13};

  binary_search_merger->Merge(locations, phrase, suffix, prefix_locs.begin(),
      prefix_locs.end(), suffix_locs.begin(), suffix_locs.end(), 1, 1);

  vector<int> expected_locations{2, 4, 2, 8, 2, 13, 6, 8, 6, 13, 10, 13};
  EXPECT_EQ(expected_locations, locations);
}

TEST_F(BinarySearchMergerTest, aXbXcTest) {
  vector<int> locations;
  // Encoding for it X him X it (see Adam's dissertation).
  vector<int> symbols{1, -1, 2, -2, 1};
  Phrase phrase = phrase_builder->Build(symbols);
  vector<int> suffix_symbols{-1, 2, -2, 1};
  Phrase suffix = phrase_builder->Build(suffix_symbols);

  vector<int> prefix_locs{0, 2, 0, 6, 0, 10, 4, 6, 4, 10, 4, 15, 8, 10, 8, 15,
                          13, 15};
  vector<int> suffix_locs{2, 4, 2, 8, 2, 13, 6, 8, 6, 13, 10, 13};

  binary_search_merger->Merge(locations, phrase, suffix, prefix_locs.begin(),
      prefix_locs.end(), suffix_locs.begin(), suffix_locs.end(), 2, 2);

  vector<int> expected_locs{0, 2, 4, 0, 2, 8, 0, 2, 13, 0, 6, 8, 0, 6, 13, 0,
                            10, 13, 4, 6, 8, 4, 6, 13, 4, 10, 13, 8, 10, 13};
  EXPECT_EQ(expected_locs, locations);
}

TEST_F(BinarySearchMergerTest, abXcXdTest) {
  // Sentence: Anna has many many nuts and sour apples and juicy apples.
  // Phrase: Anna has X and X apples.
  vector<int> locations;
  vector<int> symbols{1, 2, -1, 3, -2, 4};
  Phrase phrase = phrase_builder->Build(symbols);
  vector<int> suffix_symbols{2, -1, 3, -2, 4};
  Phrase suffix = phrase_builder->Build(suffix_symbols);

  vector<int> prefix_locs{1, 6, 1, 9};
  vector<int> suffix_locs{2, 6, 8, 2, 6, 11, 2, 9, 11};

  binary_search_merger->Merge(locations, phrase, suffix, prefix_locs.begin(),
      prefix_locs.end(), suffix_locs.begin(), suffix_locs.end(), 2, 3);

  vector<int> expected_locs{1, 6, 8, 1, 6, 11, 1, 9, 11};
  EXPECT_EQ(expected_locs, locations);
}

TEST_F(BinarySearchMergerTest, LargeTest) {
  vector<int> locations;
  vector<int> symbols{1, -1, 2};
  Phrase phrase = phrase_builder->Build(symbols);
  vector<int> suffix_symbols{-1, 2};
  Phrase suffix = phrase_builder->Build(suffix_symbols);

  vector<int> prefix_locs;
  for (int i = 0; i < 100; ++i) {
    prefix_locs.push_back(i * 20 + 1);
  }
  vector<int> suffix_locs;
  for (int i = 0; i < 100; ++i) {
    suffix_locs.push_back(i * 20 + 5);
    suffix_locs.push_back(i * 20 + 13);
  }

  binary_search_merger->Merge(locations, phrase, suffix, prefix_locs.begin(),
      prefix_locs.end(), suffix_locs.begin(), suffix_locs.end(), 1, 1);

  EXPECT_EQ(400, locations.size());
  for (int i = 0; i < 100; ++i) {
    EXPECT_EQ(i * 20 + 1, locations[4 * i]);
    EXPECT_EQ(i * 20 + 5, locations[4 * i + 1]);
    EXPECT_EQ(i * 20 + 1, locations[4 * i + 2]);
    EXPECT_EQ(i * 20 + 13, locations[4 * i + 3]);
  }
}

TEST_F(BinarySearchMergerTest, EmptyResultTest) {
  vector<int> locations;
  vector<int> symbols{1, -1, 2};
  Phrase phrase = phrase_builder->Build(symbols);
  vector<int> suffix_symbols{-1, 2};
  Phrase suffix = phrase_builder->Build(suffix_symbols);

  vector<int> prefix_locs;
  for (int i = 0; i < 100; ++i) {
    prefix_locs.push_back(i * 200 + 1);
  }
  vector<int> suffix_locs;
  for (int i = 0; i < 100; ++i) {
    suffix_locs.push_back(i * 200 + 101);
  }

  binary_search_merger->Merge(locations, phrase, suffix, prefix_locs.begin(),
      prefix_locs.end(), suffix_locs.begin(), suffix_locs.end(), 1, 1);

  EXPECT_EQ(0, locations.size());
}

}  // namespace
