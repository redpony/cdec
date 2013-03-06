#include <gmock/gmock.h>

#include <memory>
#include <string>

#include "data_array.h"
#include "phrase_location.h"
#include "suffix_array.h"

using namespace std;

namespace extractor {

class MockSuffixArray : public SuffixArray {
 public:
  MOCK_CONST_METHOD0(GetSize, int());
  MOCK_CONST_METHOD0(GetData, shared_ptr<DataArray>());
  MOCK_CONST_METHOD0(BuildLCPArray, vector<int>());
  MOCK_CONST_METHOD1(GetSuffix, int(int));
  MOCK_CONST_METHOD4(Lookup, PhraseLocation(int, int, const string& word, int));
};

} // namespace extractor
