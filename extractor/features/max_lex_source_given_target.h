#ifndef _MAX_LEX_SOURCE_GIVEN_TARGET_H_
#define _MAX_LEX_SOURCE_GIVEN_TARGET_H_

#include <memory>

#include "feature.h"

using namespace std;

class TranslationTable;

class MaxLexSourceGivenTarget : public Feature {
 public:
  MaxLexSourceGivenTarget(shared_ptr<TranslationTable> table);

  double Score(const FeatureContext& context) const;

  string GetName() const;

 private:
  shared_ptr<TranslationTable> table;
};

#endif
