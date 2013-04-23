#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "grammar.h"
#include "mocks/mock_fast_intersector.h"
#include "mocks/mock_matchings_finder.h"
#include "mocks/mock_rule_extractor.h"
#include "mocks/mock_sampler.h"
#include "mocks/mock_scorer.h"
#include "mocks/mock_vocabulary.h"
#include "phrase_builder.h"
#include "phrase_location.h"
#include "rule_factory.h"

using namespace std;
using namespace ::testing;

namespace extractor {
namespace {

class RuleFactoryTest : public Test {
 protected:
  virtual void SetUp() {
    finder = make_shared<MockMatchingsFinder>();
    fast_intersector = make_shared<MockFastIntersector>();

    vocabulary = make_shared<MockVocabulary>();
    EXPECT_CALL(*vocabulary, GetTerminalValue(2)).WillRepeatedly(Return("a"));
    EXPECT_CALL(*vocabulary, GetTerminalValue(3)).WillRepeatedly(Return("b"));
    EXPECT_CALL(*vocabulary, GetTerminalValue(4)).WillRepeatedly(Return("c"));

    phrase_builder = make_shared<PhraseBuilder>(vocabulary);

    scorer = make_shared<MockScorer>();
    feature_names = {"f1"};
    EXPECT_CALL(*scorer, GetFeatureNames())
        .WillRepeatedly(Return(feature_names));

    sampler = make_shared<MockSampler>();
    EXPECT_CALL(*sampler, Sample(_))
        .WillRepeatedly(Return(PhraseLocation(0, 1)));

    Phrase phrase;
    vector<double> scores = {0.5};
    vector<pair<int, int> > phrase_alignment = {make_pair(0, 0)};
    vector<Rule> rules = {Rule(phrase, phrase, scores, phrase_alignment)};
    extractor = make_shared<MockRuleExtractor>();
    EXPECT_CALL(*extractor, ExtractRules(_, _))
        .WillRepeatedly(Return(rules));
  }

  vector<string> feature_names;
  shared_ptr<MockMatchingsFinder> finder;
  shared_ptr<MockFastIntersector> fast_intersector;
  shared_ptr<MockVocabulary> vocabulary;
  shared_ptr<PhraseBuilder> phrase_builder;
  shared_ptr<MockScorer> scorer;
  shared_ptr<MockSampler> sampler;
  shared_ptr<MockRuleExtractor> extractor;
  shared_ptr<HieroCachingRuleFactory> factory;
};

TEST_F(RuleFactoryTest, TestGetGrammarDifferentWords) {
  factory = make_shared<HieroCachingRuleFactory>(finder, fast_intersector,
      phrase_builder, extractor, vocabulary, sampler, scorer, 1, 10, 2, 3, 5);

  EXPECT_CALL(*finder, Find(_, _, _))
      .Times(6)
      .WillRepeatedly(Return(PhraseLocation(0, 1)));

  EXPECT_CALL(*fast_intersector, Intersect(_, _, _))
      .Times(1)
      .WillRepeatedly(Return(PhraseLocation(0, 1)));

  vector<int> word_ids = {2, 3, 4};
  Grammar grammar = factory->GetGrammar(word_ids);
  EXPECT_EQ(feature_names, grammar.GetFeatureNames());
  EXPECT_EQ(7, grammar.GetRules().size());
}

TEST_F(RuleFactoryTest, TestGetGrammarRepeatingWords) {
  factory = make_shared<HieroCachingRuleFactory>(finder, fast_intersector,
      phrase_builder, extractor, vocabulary, sampler, scorer, 1, 10, 2, 3, 5);

  EXPECT_CALL(*finder, Find(_, _, _))
      .Times(12)
      .WillRepeatedly(Return(PhraseLocation(0, 1)));

  EXPECT_CALL(*fast_intersector, Intersect(_, _, _))
      .Times(16)
      .WillRepeatedly(Return(PhraseLocation(0, 1)));

  vector<int> word_ids = {2, 3, 4, 2, 3};
  Grammar grammar = factory->GetGrammar(word_ids);
  EXPECT_EQ(feature_names, grammar.GetFeatureNames());
  EXPECT_EQ(28, grammar.GetRules().size());
}

} // namespace
} // namespace extractor
