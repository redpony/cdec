#include <gmock/gmock.h>

#include "phrase.h"
#include "phrase_builder.h"
#include "rule.h"
#include "rule_extractor.h"

namespace extractor {

class MockRuleExtractor : public RuleExtractor {
 public:
  MOCK_CONST_METHOD2(ExtractRules, vector<Rule>(const Phrase&,
      const PhraseLocation&));
};

} // namespace extractor
