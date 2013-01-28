#include <gmock/gmock.h>

#include <string>

#include "../data_array.h"
#include "../phrase_location.h"
#include "../suffix_array.h"

using namespace std;

class MockSuffixArray : public SuffixArray {
 public:
  MockSuffixArray() : SuffixArray(make_shared<DataArray>()) {}

  MOCK_CONST_METHOD0(GetSize, int());
  MOCK_CONST_METHOD4(Lookup, PhraseLocation(int, int, const string& word, int));
};
