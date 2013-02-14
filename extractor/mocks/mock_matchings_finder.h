#include <gmock/gmock.h>

#include "../matchings_finder.h"
#include "../phrase_location.h"

class MockMatchingsFinder : public MatchingsFinder {
 public:
  MOCK_METHOD3(Find, PhraseLocation(PhraseLocation&, const string&, int));
};
