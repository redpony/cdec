#include <gmock/gmock.h>

#include "../precomputation.h"

class MockPrecomputation : public Precomputation {
 public:
  MOCK_CONST_METHOD0(GetInvertedIndex, const Index&());
  MOCK_CONST_METHOD0(GetCollocations, const Index&());
};
