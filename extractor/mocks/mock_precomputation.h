#include <gmock/gmock.h>

#include "precomputation.h"

namespace extractor {

class MockPrecomputation : public Precomputation {
 public:
  MOCK_CONST_METHOD1(Contains, bool(const vector<int>& pattern));
  MOCK_CONST_METHOD1(GetCollocations, vector<int>(const vector<int>& pattern));
};

} // namespace extractor
