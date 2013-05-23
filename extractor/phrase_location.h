#ifndef _PHRASE_LOCATION_H_
#define _PHRASE_LOCATION_H_

#include <memory>
#include <vector>

using namespace std;

namespace extractor {

/**
 * Structure containing information about the occurrences of a phrase in the
 * source data.
 *
 * Every consecutive (disjoint) group of num_subpatterns entries in matchings
 * vector encodes an occurrence of the phrase. The i-th entry of a group
 * represents the start of the i-th subpattern of the phrase. If the phrase
 * doesn't contain any nonterminals, then it may also be represented as the
 * range in the suffix array which matches the phrase.
 */
struct PhraseLocation {
  PhraseLocation(int sa_low = -1, int sa_high = -1);

  PhraseLocation(const vector<int>& matchings, int num_subpatterns);

  // Checks if a phrase has any occurrences in the source data.
  bool IsEmpty() const;

  // Returns the number of occurrences of a phrase in the source data.
  int GetSize() const;

  friend bool operator==(const PhraseLocation& a, const PhraseLocation& b);

  int sa_low, sa_high;
  shared_ptr<vector<int>> matchings;
  int num_subpatterns;
};

} // namespace extractor

#endif
