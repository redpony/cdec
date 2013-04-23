#ifndef _MATCHINGS_FINDER_H_
#define _MATCHINGS_FINDER_H_

#include <memory>
#include <string>

using namespace std;

namespace extractor {

class PhraseLocation;
class SuffixArray;

/**
 * Class wrapping the suffix array lookup for a contiguous phrase.
 */
class MatchingsFinder {
 public:
  MatchingsFinder(shared_ptr<SuffixArray> suffix_array);

  virtual ~MatchingsFinder();

  // Uses the suffix array to search only for the last word of the phrase
  // starting from the range in which the prefix of the phrase occurs.
  virtual PhraseLocation Find(PhraseLocation& location, const string& word,
                              int offset);

 protected:
  MatchingsFinder();

 private:
  shared_ptr<SuffixArray> suffix_array;
};

} // namespace extractor

#endif
