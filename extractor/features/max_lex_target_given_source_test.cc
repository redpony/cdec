#include <gtest/gtest.h>

#include <cmath>
#include <memory>
#include <string>

#include "data_array.h"
#include "mocks/mock_translation_table.h"
#include "mocks/mock_vocabulary.h"
#include "phrase_builder.h"
#include "max_lex_target_given_source.h"

using namespace std;
using namespace ::testing;

namespace extractor {
namespace features {
namespace {

class MaxLexTargetGivenSourceTest : public Test {
 protected:
  virtual void SetUp() {
    vector<string> source_words = {"f1", "f2", "f3"};
    vector<string> target_words = {"e1", "e2", "e3"};

    vocabulary = make_shared<MockVocabulary>();
    for (size_t i = 0; i < source_words.size(); ++i) {
      EXPECT_CALL(*vocabulary, GetTerminalValue(i))
          .WillRepeatedly(Return(source_words[i]));
    }
    for (size_t i = 0; i < target_words.size(); ++i) {
      EXPECT_CALL(*vocabulary, GetTerminalValue(i + source_words.size()))
          .WillRepeatedly(Return(target_words[i]));
    }

    phrase_builder = make_shared<PhraseBuilder>(vocabulary);

    table = make_shared<MockTranslationTable>();
    for (size_t i = 0; i < source_words.size(); ++i) {
      for (size_t j = 0; j < target_words.size(); ++j) {
        int value = i - j;
        EXPECT_CALL(*table, GetTargetGivenSourceScore(
            source_words[i], target_words[j])).WillRepeatedly(Return(value));
      }
    }

    for (size_t i = 0; i < target_words.size(); ++i) {
      int value = i * 3;
      EXPECT_CALL(*table, GetTargetGivenSourceScore(
          DataArray::NULL_WORD_STR, target_words[i]))
          .WillRepeatedly(Return(value));
    }

    feature = make_shared<MaxLexTargetGivenSource>(table);
  }

  shared_ptr<MockVocabulary> vocabulary;
  shared_ptr<PhraseBuilder> phrase_builder;
  shared_ptr<MockTranslationTable> table;
  shared_ptr<MaxLexTargetGivenSource> feature;
};

TEST_F(MaxLexTargetGivenSourceTest, TestGetName) {
  EXPECT_EQ("MaxLexEgivenF", feature->GetName());
}

TEST_F(MaxLexTargetGivenSourceTest, TestScore) {
  vector<int> source_symbols = {0, 1, 2};
  Phrase source_phrase = phrase_builder->Build(source_symbols);
  vector<int> target_symbols = {3, 4, 5};
  Phrase target_phrase = phrase_builder->Build(target_symbols);
  FeatureContext context(source_phrase, target_phrase, 0.3, 7, 19);
  EXPECT_EQ(-log10(36), feature->Score(context));
}

} // namespace
} // namespace features
} // namespace extractor
