#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include "mocks/mock_alignment.h"
#include "mocks/mock_data_array.h"
#include "mocks/mock_rule_extractor_helper.h"
#include "mocks/mock_vocabulary.h"
#include "phrase.h"
#include "phrase_builder.h"
#include "target_phrase_extractor.h"

using namespace std;
using namespace ::testing;

namespace extractor {
namespace {

class TargetPhraseExtractorTest : public Test {
 protected:
  virtual void SetUp() {
    data_array = make_shared<MockDataArray>();
    alignment = make_shared<MockAlignment>();
    vocabulary = make_shared<MockVocabulary>();
    phrase_builder = make_shared<PhraseBuilder>(vocabulary);
    helper = make_shared<MockRuleExtractorHelper>();
  }

  shared_ptr<MockDataArray> data_array;
  shared_ptr<MockAlignment> alignment;
  shared_ptr<MockVocabulary> vocabulary;
  shared_ptr<PhraseBuilder> phrase_builder;
  shared_ptr<MockRuleExtractorHelper> helper;
  shared_ptr<TargetPhraseExtractor> extractor;
};

TEST_F(TargetPhraseExtractorTest, TestExtractTightPhrasesTrue) {
  EXPECT_CALL(*data_array, GetSentenceLength(1)).WillRepeatedly(Return(5));
  EXPECT_CALL(*data_array, GetSentenceStart(1)).WillRepeatedly(Return(3));

  vector<string> target_words = {"a", "b", "c", "d", "e"};
  vector<int> target_symbols = {20, 21, 22, 23, 24};
  for (size_t i = 0; i < target_words.size(); ++i) {
    EXPECT_CALL(*data_array, GetWordAtIndex(i + 3))
        .WillRepeatedly(Return(target_words[i]));
    EXPECT_CALL(*vocabulary, GetTerminalIndex(target_words[i]))
        .WillRepeatedly(Return(target_symbols[i]));
    EXPECT_CALL(*vocabulary, GetTerminalValue(target_symbols[i]))
        .WillRepeatedly(Return(target_words[i]));
  }

  vector<pair<int, int> > links = {
    make_pair(0, 0), make_pair(1, 3), make_pair(2, 2), make_pair(3, 1),
    make_pair(4, 4)
  };
  EXPECT_CALL(*alignment, GetLinks(1)).WillRepeatedly(Return(links));

  vector<int> gap_order = {1, 0};
  EXPECT_CALL(*helper, GetGapOrder(_)).WillRepeatedly(Return(gap_order));

  extractor = make_shared<TargetPhraseExtractor>(
      data_array, alignment, phrase_builder, helper, vocabulary, 10, true);

  vector<pair<int, int> > target_gaps = {make_pair(3, 4), make_pair(1, 2)};
  vector<int> target_low = {0, 3, 2, 1, 4};
  unordered_map<int, int> source_indexes = {{0, 0}, {2, 2}, {4, 4}};

  vector<pair<Phrase, PhraseAlignment> > results =  extractor->ExtractPhrases(
      target_gaps, target_low, 0, 5, source_indexes, 1);
  EXPECT_EQ(1, results.size());
  vector<int> expected_symbols = {20, -2, 22, -1, 24};
  EXPECT_EQ(expected_symbols, results[0].first.Get());
  vector<string> expected_words = {"a", "c", "e"};
  EXPECT_EQ(expected_words, results[0].first.GetWords());
  vector<pair<int, int> > expected_alignment = {
    make_pair(0, 0), make_pair(2, 2), make_pair(4, 4)
  };
  EXPECT_EQ(expected_alignment, results[0].second);
}

TEST_F(TargetPhraseExtractorTest, TestExtractPhrasesTightPhrasesFalse) {
  vector<string> target_words = {"a", "b", "c", "d", "e", "f", "END_OF_LINE"};
  vector<int> target_symbols = {20, 21, 22, 23, 24, 25, 1};
  EXPECT_CALL(*data_array, GetSentenceLength(0)).WillRepeatedly(Return(6));
  EXPECT_CALL(*data_array, GetSentenceStart(0)).WillRepeatedly(Return(0));

  for (size_t i = 0; i < target_words.size(); ++i) {
    EXPECT_CALL(*data_array, GetWordAtIndex(i))
        .WillRepeatedly(Return(target_words[i]));
    EXPECT_CALL(*vocabulary, GetTerminalIndex(target_words[i]))
        .WillRepeatedly(Return(target_symbols[i]));
    EXPECT_CALL(*vocabulary, GetTerminalValue(target_symbols[i]))
        .WillRepeatedly(Return(target_words[i]));
  }

  vector<pair<int, int> > links = {make_pair(1, 1)};
  EXPECT_CALL(*alignment, GetLinks(0)).WillRepeatedly(Return(links));

  vector<int> gap_order = {0};
  EXPECT_CALL(*helper, GetGapOrder(_)).WillRepeatedly(Return(gap_order));

  extractor = make_shared<TargetPhraseExtractor>(
      data_array, alignment, phrase_builder, helper, vocabulary, 10, false);

  vector<pair<int, int> > target_gaps = {make_pair(2, 4)};
  vector<int> target_low = {-1, 1, -1, -1, -1, -1};
  unordered_map<int, int> source_indexes = {{1, 1}};

  vector<pair<Phrase, PhraseAlignment> > results = extractor->ExtractPhrases(
      target_gaps, target_low, 1, 5, source_indexes, 0);
  EXPECT_EQ(10, results.size());

  for (int i = 0; i < 2; ++i) {
    for (int j = 4; j <= 6; ++j) {
      for (int k = 4; k <= j; ++k) {
        vector<string> expected_words;
        for (int l = i; l < 2; ++l) {
          expected_words.push_back(target_words[l]);
        }
        for (int l = k; l < j; ++l) {
          expected_words.push_back(target_words[l]);
        }

        PhraseAlignment expected_alignment;
        expected_alignment.push_back(make_pair(1, 1 - i));

        bool found_expected_pair = false;
        for (auto result: results) {
          if (result.first.GetWords() == expected_words &&
              result.second == expected_alignment) {
            found_expected_pair = true;
          }
        }

        EXPECT_TRUE(found_expected_pair);
      }
    }
  }
}

} // namespace
} // namespace extractor
