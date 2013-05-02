#include <gtest/gtest.h>

#include <memory>

#include "mocks/mock_alignment.h"
#include "mocks/mock_data_array.h"
#include "mocks/mock_rule_extractor_helper.h"
#include "mocks/mock_scorer.h"
#include "mocks/mock_target_phrase_extractor.h"
#include "mocks/mock_vocabulary.h"
#include "phrase.h"
#include "phrase_builder.h"
#include "phrase_location.h"
#include "rule_extractor.h"
#include "rule.h"

using namespace std;
using namespace ::testing;

namespace extractor {
namespace {

class RuleExtractorTest : public Test {
 protected:
  virtual void SetUp() {
    source_data_array = make_shared<MockDataArray>();
    EXPECT_CALL(*source_data_array, GetSentenceId(_))
        .WillRepeatedly(Return(0));
    EXPECT_CALL(*source_data_array, GetSentenceStart(_))
        .WillRepeatedly(Return(0));
    EXPECT_CALL(*source_data_array, GetSentenceLength(_))
        .WillRepeatedly(Return(10));

    helper = make_shared<MockRuleExtractorHelper>();
    EXPECT_CALL(*helper, CheckAlignedTerminals(_, _, _, _))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*helper, CheckTightPhrases(_, _, _, _))
        .WillRepeatedly(Return(true));
    unordered_map<int, int> source_indexes;
    EXPECT_CALL(*helper, GetSourceIndexes(_, _, _, _))
        .WillRepeatedly(Return(source_indexes));

    vocabulary = make_shared<MockVocabulary>();
    EXPECT_CALL(*vocabulary, GetTerminalValue(87))
        .WillRepeatedly(Return("a"));
    phrase_builder = make_shared<PhraseBuilder>(vocabulary);
    vector<int> symbols = {87};
    Phrase target_phrase = phrase_builder->Build(symbols);
    PhraseAlignment phrase_alignment = {make_pair(0, 0)};

    target_phrase_extractor = make_shared<MockTargetPhraseExtractor>();
    vector<pair<Phrase, PhraseAlignment> > target_phrases = {
      make_pair(target_phrase, phrase_alignment)
    };
    EXPECT_CALL(*target_phrase_extractor, ExtractPhrases(_, _, _, _, _, _))
        .WillRepeatedly(Return(target_phrases));

    scorer = make_shared<MockScorer>();
    vector<double> scores = {0.3, 7.2};
    EXPECT_CALL(*scorer, Score(_)).WillRepeatedly(Return(scores));

    extractor = make_shared<RuleExtractor>(source_data_array, phrase_builder,
        scorer, target_phrase_extractor, helper, 10, 1, 3, 5, false);
  }

  shared_ptr<MockDataArray> source_data_array;
  shared_ptr<MockVocabulary> vocabulary;
  shared_ptr<PhraseBuilder> phrase_builder;
  shared_ptr<MockRuleExtractorHelper> helper;
  shared_ptr<MockScorer> scorer;
  shared_ptr<MockTargetPhraseExtractor> target_phrase_extractor;
  shared_ptr<RuleExtractor> extractor;
};

TEST_F(RuleExtractorTest, TestExtractRulesAlignedTerminalsFail) {
  vector<int> symbols = {87};
  Phrase phrase = phrase_builder->Build(symbols);
  vector<int> matching = {2};
  PhraseLocation phrase_location(matching, 1);
  EXPECT_CALL(*helper, GetLinksSpans(_, _, _, _, _)).Times(1);
  EXPECT_CALL(*helper, CheckAlignedTerminals(_, _, _, _))
      .WillRepeatedly(Return(false));
  vector<Rule> rules = extractor->ExtractRules(phrase, phrase_location);
  EXPECT_EQ(0, rules.size());
}

TEST_F(RuleExtractorTest, TestExtractRulesTightPhrasesFail) {
  vector<int> symbols = {87};
  Phrase phrase = phrase_builder->Build(symbols);
  vector<int> matching = {2};
  PhraseLocation phrase_location(matching, 1);
  EXPECT_CALL(*helper, GetLinksSpans(_, _, _, _, _)).Times(1);
  EXPECT_CALL(*helper, CheckTightPhrases(_, _, _, _))
      .WillRepeatedly(Return(false));
  vector<Rule> rules = extractor->ExtractRules(phrase, phrase_location);
  EXPECT_EQ(0, rules.size());
}

TEST_F(RuleExtractorTest, TestExtractRulesNoFixPoint) {
  vector<int> symbols = {87};
  Phrase phrase = phrase_builder->Build(symbols);
  vector<int> matching = {2};
  PhraseLocation phrase_location(matching, 1);

  EXPECT_CALL(*helper, GetLinksSpans(_, _, _, _, _)).Times(1);
  // Set FindFixPoint to return false.
  vector<pair<int, int> > gaps;
  helper->SetUp(0, 0, 0, 0, false, gaps, gaps, 0, true, true);

  vector<Rule> rules = extractor->ExtractRules(phrase, phrase_location);
  EXPECT_EQ(0, rules.size());
}

TEST_F(RuleExtractorTest, TestExtractRulesGapsFail) {
  vector<int> symbols = {87};
  Phrase phrase = phrase_builder->Build(symbols);
  vector<int> matching = {2};
  PhraseLocation phrase_location(matching, 1);

  EXPECT_CALL(*helper, GetLinksSpans(_, _, _, _, _)).Times(1);
  // Set CheckGaps to return false.
  vector<pair<int, int> > gaps;
  helper->SetUp(0, 0, 0, 0, true, gaps, gaps, 0, true, false);

  vector<Rule> rules = extractor->ExtractRules(phrase, phrase_location);
  EXPECT_EQ(0, rules.size());
}

TEST_F(RuleExtractorTest, TestExtractRulesNoExtremities) {
  vector<int> symbols = {87};
  Phrase phrase = phrase_builder->Build(symbols);
  vector<int> matching = {2};
  PhraseLocation phrase_location(matching, 1);

  EXPECT_CALL(*helper, GetLinksSpans(_, _, _, _, _)).Times(1);
  vector<pair<int, int> > gaps(3);
  // Set FindFixPoint to return true. The number of gaps equals the number of
  // nonterminals, so we won't add any extremities.
  helper->SetUp(0, 0, 0, 0, true, gaps, gaps, 0, true, true);

  vector<Rule> rules = extractor->ExtractRules(phrase, phrase_location);
  EXPECT_EQ(1, rules.size());
}

TEST_F(RuleExtractorTest, TestExtractRulesAddExtremities) {
  vector<int> symbols = {87};
  Phrase phrase = phrase_builder->Build(symbols);
  vector<int> matching = {2};
  PhraseLocation phrase_location(matching, 1);

  vector<int> links(10, -1);
  EXPECT_CALL(*helper, GetLinksSpans(_, _, _, _, _)).WillOnce(DoAll(
      SetArgReferee<0>(links),
      SetArgReferee<1>(links),
      SetArgReferee<2>(links),
      SetArgReferee<3>(links)));

  vector<pair<int, int> > gaps;
  // Set FindFixPoint to return true. The number of gaps equals the number of
  // nonterminals, so we won't add any extremities.
  helper->SetUp(0, 0, 2, 3, true, gaps, gaps, 0, true, true);

  vector<Rule> rules = extractor->ExtractRules(phrase, phrase_location);
  EXPECT_EQ(4, rules.size());
}

} // namespace
} // namespace extractor
