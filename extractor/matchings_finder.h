#ifndef _MATCHINGS_FINDER_H_
#define _MATCHINGS_FINDER_H_

#include <memory>
#include <string>

using namespace std;

namespace extractor {

class PhraseLocation;
class SuffixArray;

class MatchingsFinder {
 public:
  MatchingsFinder(shared_ptr<SuffixArray> suffix_array);

  virtual ~MatchingsFinder();

  virtual PhraseLocation Find(PhraseLocation& location, const string& word,
                              int offset);

 protected:
  MatchingsFinder();

 private:
  shared_ptr<SuffixArray> suffix_array;
};

} // namespace extractor

#endif
