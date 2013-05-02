#ifndef _FAST_INTERSECTOR_H_
#define _FAST_INTERSECTOR_H_

#include <memory>
#include <unordered_map>
#include <vector>

#include <boost/functional/hash.hpp>

using namespace std;

namespace extractor {

typedef boost::hash<vector<int> > VectorHash;
typedef unordered_map<vector<int>, vector<int>, VectorHash> Index;

class Phrase;
class PhraseLocation;
class Precomputation;
class SuffixArray;
class Vocabulary;

/**
 * Component for searching the training data for occurrences of source phrases
 * containing nonterminals
 *
 * Given a source phrase containing a nonterminal, we first query the
 * precomputed index containing frequent collocations. If the phrase is not
 * frequent enough, we extend the matchings of either its prefix or its suffix,
 * depending on which operation seems to require less computations.
 *
 * Note: This method for intersecting phrase locations is faster than both
 * mergers (linear or Baeza Yates) described in Adam Lopez' dissertation.
 */
class FastIntersector {
 public:
  FastIntersector(shared_ptr<SuffixArray> suffix_array,
                  shared_ptr<Precomputation> precomputation,
                  shared_ptr<Vocabulary> vocabulary,
                  int max_rule_span,
                  int min_gap_size);

  virtual ~FastIntersector();

  // Finds the locations of a phrase given the locations of its prefix and
  // suffix.
  virtual PhraseLocation Intersect(PhraseLocation& prefix_location,
                                   PhraseLocation& suffix_location,
                                   const Phrase& phrase);

 protected:
  FastIntersector();

 private:
  // Uses the vocabulary to convert the phrase from the numberized format
  // specified by the source data array to the numberized format given by the
  // vocabulary.
  vector<int> ConvertPhrase(const vector<int>& old_phrase);

  // Estimates the number of computations needed if the prefix/suffix is
  // extended. If the last/first symbol is separated from the rest of the phrase
  // by a nonterminal, then for each occurrence of the prefix/suffix we need to
  // check max_rule_span positions. Otherwise, we only need to check a single
  // position for each occurrence.
  int EstimateNumOperations(const PhraseLocation& phrase_location,
                            bool has_margin_x) const;

  // Uses the occurrences of the prefix to find the occurrences of the phrase.
  PhraseLocation ExtendPrefixPhraseLocation(PhraseLocation& prefix_location,
                                            const Phrase& phrase,
                                            bool prefix_ends_with_x,
                                            int next_symbol) const;

  // Uses the occurrences of the suffix to find the occurrences of the phrase.
  PhraseLocation ExtendSuffixPhraseLocation(PhraseLocation& suffix_location,
                                            const Phrase& phrase,
                                            bool suffix_starts_with_x,
                                            int prev_symbol) const;

  // Extends the prefix/suffix location to a list of subpatterns positions if it
  // represents a suffix array range.
  void ExtendPhraseLocation(PhraseLocation& location) const;

  // Returns the range in which the search should be performed.
  pair<int, int> GetSearchRange(bool has_marginal_x) const;

  shared_ptr<SuffixArray> suffix_array;
  shared_ptr<Vocabulary> vocabulary;
  int max_rule_span;
  int min_gap_size;
  Index collocations;
};

} // namespace extractor

#endif
