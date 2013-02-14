#ifndef _INTERSECTOR_H_
#define _INTERSECTOR_H_

#include <memory>
#include <unordered_map>
#include <vector>

#include <boost/functional/hash.hpp>

#include "binary_search_merger.h"
#include "linear_merger.h"

using namespace std;

typedef boost::hash<vector<int> > VectorHash;
typedef unordered_map<vector<int>, vector<int>, VectorHash> Index;

class DataArray;
class MatchingComparator;
class Phrase;
class PhraseLocation;
class Precomputation;
class SuffixArray;
class Vocabulary;

class Intersector {
 public:
  Intersector(
      shared_ptr<Vocabulary> vocabulary,
      shared_ptr<Precomputation> precomputation,
      shared_ptr<SuffixArray> source_suffix_array,
      shared_ptr<MatchingComparator> comparator,
      bool use_baeza_yates);

  // For testing.
  Intersector(
      shared_ptr<Vocabulary> vocabulary,
      shared_ptr<Precomputation> precomputation,
      shared_ptr<SuffixArray> source_suffix_array,
      shared_ptr<LinearMerger> linear_merger,
      shared_ptr<BinarySearchMerger> binary_search_merger,
      bool use_baeza_yates);

  virtual ~Intersector();

  virtual PhraseLocation Intersect(
      const Phrase& prefix, PhraseLocation& prefix_location,
      const Phrase& suffix, PhraseLocation& suffix_location,
      const Phrase& phrase);

 protected:
  Intersector();

 private:
  void ConvertIndexes(shared_ptr<Precomputation> precomputation,
                      shared_ptr<DataArray> data_array);

  vector<int> ConvertPhrase(const vector<int>& old_phrase,
                            shared_ptr<DataArray> data_array);

  void ExtendPhraseLocation(const Phrase& phrase,
      PhraseLocation& phrase_location);

  shared_ptr<Vocabulary> vocabulary;
  shared_ptr<SuffixArray> suffix_array;
  shared_ptr<LinearMerger> linear_merger;
  shared_ptr<BinarySearchMerger> binary_search_merger;
  Index inverted_index;
  Index collocations;
  bool use_baeza_yates;

  // TODO(pauldb): Don't forget to remove these.
 public:
  double sort_time;
  double linear_merge_time;
  double binary_merge_time;
};

#endif
