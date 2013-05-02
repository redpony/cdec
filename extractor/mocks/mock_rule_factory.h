#include <gmock/gmock.h>

#include "grammar.h"
#include "rule_factory.h"

namespace extractor {

class MockHieroCachingRuleFactory : public HieroCachingRuleFactory {
 public:
  MOCK_METHOD1(GetGrammar, Grammar(const vector<int>& word_ids));
};

} // namespace extractor
