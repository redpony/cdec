#ifndef _MAX_LEX_TARGET_GIVEN_SOURCE_H_
#define _MAX_LEX_TARGET_GIVEN_SOURCE_H_

#include <memory>

#include "feature.h"

using namespace std;

namespace extractor {

class TranslationTable;

namespace features {

/**
 * Feature computing max(p(e | f)) across all pairs of words in the phrase pair.
 */
class MaxLexTargetGivenSource : public Feature {
 public:
  MaxLexTargetGivenSource(shared_ptr<TranslationTable> table);

  double Score(const FeatureContext& context) const;

  string GetName() const;

 private:
  shared_ptr<TranslationTable> table;
};

} // namespace features
} // namespace extractor

#endif
