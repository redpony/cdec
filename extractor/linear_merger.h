#ifndef _LINEAR_MERGER_H_
#define _LINEAR_MERGER_H_

#include <memory>
#include <vector>

using namespace std;

class MatchingComparator;
class Phrase;
class PhraseLocation;
class DataArray;
class Vocabulary;

class LinearMerger {
 public:
  LinearMerger(shared_ptr<Vocabulary> vocabulary,
               shared_ptr<DataArray> data_array,
               shared_ptr<MatchingComparator> comparator);

  virtual ~LinearMerger();

  virtual void Merge(
      vector<int>& locations, const Phrase& phrase, const Phrase& suffix,
      vector<int>::iterator prefix_start, vector<int>::iterator prefix_end,
      vector<int>::iterator suffix_start, vector<int>::iterator suffix_end,
      int prefix_subpatterns, int suffix_subpatterns);

 protected:
  LinearMerger();

 private:
  shared_ptr<Vocabulary> vocabulary;
  shared_ptr<DataArray> data_array;
  shared_ptr<MatchingComparator> comparator;

  // TODO(pauldb): Remove this eventually.
 public:
  double linear_merge_time;
};

#endif
