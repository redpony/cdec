#ifndef _BINARY_SEARCH_MERGER_H_
#define _BINARY_SEARCH_MERGER_H_

#include <memory>
#include <vector>

using namespace std;

class DataArray;
class LinearMerger;
class MatchingComparator;
class Phrase;
class Vocabulary;

class BinarySearchMerger {
 public:
  BinarySearchMerger(shared_ptr<Vocabulary> vocabulary,
                     shared_ptr<LinearMerger> linear_merger,
                     shared_ptr<DataArray> data_array,
                     shared_ptr<MatchingComparator> comparator,
                     bool force_binary_search_merge = false);

  void Merge(
      vector<int>& locations, const Phrase& phrase, const Phrase& suffix,
      vector<int>::iterator prefix_start, vector<int>::iterator prefix_end,
      vector<int>::iterator suffix_start, vector<int>::iterator suffix_end,
      int prefix_subpatterns, int suffix_subpatterns) const;

  static double BAEZA_YATES_FACTOR;

 private:
  bool IsIntersectionVoid(
      vector<int>::iterator prefix_start, vector<int>::iterator prefix_end,
      vector<int>::iterator suffix_start, vector<int>::iterator suffix_end,
      int prefix_subpatterns, int suffix_subpatterns,
      const Phrase& suffix) const;

  bool ShouldUseLinearMerge(int prefix_set_size, int suffix_set_size) const;

  vector<int>::iterator GetMiddle(vector<int>::iterator low,
                                  vector<int>::iterator high,
                                  int num_subpatterns) const;

  void GetComparableMatchings(
      const vector<int>::iterator& prefix_start,
      const vector<int>::iterator& prefix_end,
      const vector<int>::iterator& prefix_mid,
      int num_subpatterns,
      vector<int>::iterator& prefix_low,
      vector<int>::iterator& prefix_high) const;

  int CompareMatchingsSet(
      const vector<int>::iterator& prefix_low,
      const vector<int>::iterator& prefix_high,
      const vector<int>::iterator& suffix_mid,
      int prefix_subpatterns,
      int suffix_subpatterns,
      const Phrase& suffix) const;

  shared_ptr<Vocabulary> vocabulary;
  shared_ptr<LinearMerger> linear_merger;
  shared_ptr<DataArray> data_array;
  shared_ptr<MatchingComparator> comparator;
  // Should be true only for testing.
  bool force_binary_search_merge;
};

#endif
