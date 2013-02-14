#include <gmock/gmock.h>

#include "../phrase_location.h"
#include "../sampler.h"

class MockSampler : public Sampler {
 public:
  MOCK_CONST_METHOD1(Sample, PhraseLocation(const PhraseLocation& location));
};
