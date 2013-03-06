#include <gmock/gmock.h>

#include "alignment.h"

namespace extractor {

typedef vector<pair<int, int> > SentenceLinks;

class MockAlignment : public Alignment {
 public:
  MOCK_CONST_METHOD1(GetLinks, SentenceLinks(int sentence_id));
};

} // namespace extractor
