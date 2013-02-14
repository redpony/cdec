#include <gmock/gmock.h>

#include <vector>

#include "../linear_merger.h"
#include "../phrase.h"

using namespace std;

class MockLinearMerger: public LinearMerger {
 public:
  MOCK_METHOD9(Merge, void(vector<int>&, const Phrase&, const Phrase&,
      vector<int>::iterator, vector<int>::iterator, vector<int>::iterator,
      vector<int>::iterator, int, int));
};
