#include <gmock/gmock.h>

#include <vector>

#include "linear_merger.h"
#include "phrase.h"

using namespace std;

class MockLinearMerger: public LinearMerger {
 public:
  MockLinearMerger(shared_ptr<Vocabulary> vocabulary,
                   shared_ptr<DataArray> data_array,
                   shared_ptr<MatchingComparator> comparator) :
      LinearMerger(vocabulary, data_array, comparator) {}


  MOCK_CONST_METHOD9(Merge, void(vector<int>&, const Phrase&, const Phrase&,
      vector<int>::iterator, vector<int>::iterator, vector<int>::iterator,
      vector<int>::iterator, int, int));
};
