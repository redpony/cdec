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

namespace extractor {

typedef boost::hash<vector<int> > VectorHash;
typedef unordered_map<vector<int>, vector<int>, VectorHash> Index;

class SuffixArray;

/**
 * Data structure wrapping an index with all the occurrences of the most
 * frequent discontiguous collocations in the source data.
 *
 * Let a, b, c be contiguous collocations. The index will contain an entry for
 * every collocation of the form:
 * - aXb, where a and b are frequent
 * - aXbXc, where a and b are super-frequent and c is frequent or
 *                b and c are super-frequent and a is frequent.
 */
class Precomputation {
 public:
  // Constructs the index using the suffix array.
  Precomputation(
      shared_ptr<SuffixArray> suffix_array, int num_frequent_patterns,
      int num_super_frequent_patterns, int max_rule_span,
      int max_rule_symbols, int min_gap_size,
      int max_frequent_phrase_len, int min_frequency);

  virtual ~Precomputation();

  void WriteBinary(const fs::path& filepath) const;

  // Returns a reference to the index.
  virtual const Index& GetCollocations() const;

  static int FIRST_NONTERMINAL;
  static int SECOND_NONTERMINAL;

 protected:
  Precomputation();

 private:
  // Finds the most frequent contiguous collocations.
  vector<vector<int> > FindMostFrequentPatterns(
      shared_ptr<SuffixArray> suffix_array, const vector<int>& data,
      int num_frequent_patterns, int max_frequent_phrase_len,
      int min_frequency);

  // Given the locations of the frequent contiguous collocations in a sentence,
  // it adds new entries to the index for each discontiguous collocation
  // matching the criteria specified in the class description.
  void AddCollocations(
      const vector<std::tuple<int, int, int> >& matchings, const vector<int>& data,
      int max_rule_span, int min_gap_size, int max_rule_symbols);

  // Adds an occurrence of a binary collocation.
  void AddStartPositions(vector<int>& positions, int pos1, int pos2);

  // Adds an occurrence of a ternary collocation.
  void AddStartPositions(vector<int>& positions, int pos1, int pos2, int pos3);

  Index collocations;
};

} // namespace extractor

#endif
