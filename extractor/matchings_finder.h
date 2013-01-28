#ifndef _MATCHINGS_FINDER_H_
#define _MATCHINGS_FINDER_H_

#include <memory>
#include <string>

using namespace std;

class PhraseLocation;
class SuffixArray;

class MatchingsFinder {
 public:
  MatchingsFinder(shared_ptr<SuffixArray> suffix_array);

  PhraseLocation Find(PhraseLocation& location, const string& word, int offset);

 private:
  shared_ptr<SuffixArray> suffix_array;
};

#endif
