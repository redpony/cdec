#include <gmock/gmock.h>

#include "../intersector.h"
#include "../phrase.h"
#include "../phrase_location.h"

class MockIntersector : public Intersector {
 public:
  MOCK_METHOD5(Intersect, PhraseLocation(const Phrase&, PhraseLocation&,
      const Phrase&, PhraseLocation&, const Phrase&));
};
