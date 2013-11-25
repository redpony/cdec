#include <gmock/gmock.h>

#include "grammar.h"
#include "rule_factory.h"

namespace extractor {

class MockHieroCachingRuleFactory : public HieroCachingRuleFactory {
 public:
  MOCK_METHOD2(GetGrammar, Grammar(
      const vector<int>& word_ids,
      const unordered_set<int>& blacklisted_sentence_ids));
};

} // namespace extractor
