#include <gmock/gmock.h>

#include "translation_table.h"

namespace extractor {

class MockTranslationTable : public TranslationTable {
 public:
  MOCK_METHOD2(GetSourceGivenTargetScore, double(const string&, const string&));
  MOCK_METHOD2(GetTargetGivenSourceScore, double(const string&, const string&));
};

} // namespace extractor
