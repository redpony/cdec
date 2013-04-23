#include <gmock/gmock.h>

#include "fast_intersector.h"
#include "phrase.h"
#include "phrase_location.h"

namespace extractor {

class MockFastIntersector : public FastIntersector {
 public:
  MOCK_METHOD3(Intersect, PhraseLocation(PhraseLocation&, PhraseLocation&,
                                         const Phrase&));
};

} // namespace extractor
