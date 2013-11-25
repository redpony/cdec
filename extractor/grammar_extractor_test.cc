#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "grammar.h"
#include "grammar_extractor.h"
#include "mocks/mock_rule_factory.h"
#include "mocks/mock_vocabulary.h"
#include "rule.h"

using namespace std;
using namespace ::testing;

namespace extractor {
namespace {

TEST(GrammarExtractorTest, TestAnnotatingWords) {
  shared_ptr<MockVocabulary> vocabulary = make_shared<MockVocabulary>();
  EXPECT_CALL(*vocabulary, GetTerminalIndex("<s>"))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*vocabulary, GetTerminalIndex("Anna"))
      .WillRepeatedly(Return(1));
  EXPECT_CALL(*vocabulary, GetTerminalIndex("has"))
      .WillRepeatedly(Return(2));
  EXPECT_CALL(*vocabulary, GetTerminalIndex("many"))
      .WillRepeatedly(Return(3));
  EXPECT_CALL(*vocabulary, GetTerminalIndex("apples"))
      .WillRepeatedly(Return(4));
  EXPECT_CALL(*vocabulary, GetTerminalIndex("."))
      .WillRepeatedly(Return(5));
  EXPECT_CALL(*vocabulary, GetTerminalIndex("</s>"))
      .WillRepeatedly(Return(6));

  shared_ptr<MockHieroCachingRuleFactory> factory =
      make_shared<MockHieroCachingRuleFactory>();
  vector<int> word_ids = {0, 1, 2, 3, 3, 4, 5, 6};
  vector<Rule> rules;
  vector<string> feature_names;
  Grammar grammar(rules, feature_names);
  unordered_set<int> blacklisted_sentence_ids;
  shared_ptr<DataArray> source_data_array;
  EXPECT_CALL(*factory, GetGrammar(word_ids, blacklisted_sentence_ids))
      .WillOnce(Return(grammar));

  GrammarExtractor extractor(vocabulary, factory);
  string sentence = "Anna has many many apples .";

  extractor.GetGrammar(sentence, blacklisted_sentence_ids);
}

} // namespace
} // namespace extractor
