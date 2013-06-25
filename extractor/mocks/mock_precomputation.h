#include <gmock/gmock.h>

#include "precomputation.h"

namespace extractor {

class MockPrecomputation : public Precomputation {
 public:
  MOCK_CONST_METHOD0(GetCollocations, Collocations());
};

} // namespace extractor
