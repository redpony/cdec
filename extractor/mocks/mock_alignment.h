#include <gmock/gmock.h>

#include "../alignment.h"

typedef vector<pair<int, int> > SentenceLinks;

class MockAlignment : public Alignment {
 public:
  MOCK_CONST_METHOD1(GetLinks, SentenceLinks(int sentence_id));
};
