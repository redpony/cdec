#ifndef _PRECOMPUTATION_H_
#define _PRECOMPUTATION_H_

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <tuple>
#include <vector>

#include <boost/filesystem.hpp>
#include <boost/functional/hash.hpp>

namespace fs = boost::filesystem;
using namespace std;

class SuffixArray;

typedef boost::hash<vector<int> > VectorHash;
typedef unordered_map<vector<int>, vector<int>, VectorHash> Index;

class Precomputation {
 public:
  Precomputation(
      shared_ptr<SuffixArray> suffix_array, int num_frequent_patterns,
      int num_super_frequent_patterns, int max_rule_span,
      int max_rule_symbols, int min_gap_size,
      int max_frequent_phrase_len, int min_frequency);

  virtual ~Precomputation();

  void WriteBinary(const fs::path& filepath) const;

  virtual const Index& GetInvertedIndex() const;
  virtual const Index& GetCollocations() const;

  static int NON_TERMINAL;

 protected:
  Precomputation();

 private:
  vector<vector<int> > FindMostFrequentPatterns(
      shared_ptr<SuffixArray> suffix_array, const vector<int>& data,
      int num_frequent_patterns, int max_frequent_phrase_len,
      int min_frequency);
  void AddCollocations(
      const vector<std::tuple<int, int, int> >& matchings, const vector<int>& data,
      int max_rule_span, int min_gap_size, int max_rule_symbols);
  void AddStartPositions(vector<int>& positions, int pos1, int pos2);
  void AddStartPositions(vector<int>& positions, int pos1, int pos2, int pos3);

  Index inverted_index;
  Index collocations;
};

#endif
