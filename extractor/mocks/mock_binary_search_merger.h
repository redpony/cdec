#include <gmock/gmock.h>

#include <vector>

#include "../binary_search_merger.h"
#include "../phrase.h"

using namespace std;

class MockBinarySearchMerger: public BinarySearchMerger {
 public:
  MOCK_CONST_METHOD9(Merge, void(vector<int>&, const Phrase&, const Phrase&,
      vector<int>::iterator, vector<int>::iterator, vector<int>::iterator,
      vector<int>::iterator, int, int));
};
