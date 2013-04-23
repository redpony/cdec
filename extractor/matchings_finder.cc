#include "matchings_finder.h"

#include "suffix_array.h"
#include "phrase_location.h"

namespace extractor {

MatchingsFinder::MatchingsFinder(shared_ptr<SuffixArray> suffix_array) :
    suffix_array(suffix_array) {}

MatchingsFinder::MatchingsFinder() {}

MatchingsFinder::~MatchingsFinder() {}

PhraseLocation MatchingsFinder::Find(PhraseLocation& location,
                                     const string& word, int offset) {
  if (location.sa_low == -1 && location.sa_high == -1) {
    location.sa_low = 0;
    location.sa_high = suffix_array->GetSize();
  }

  return suffix_array->Lookup(location.sa_low, location.sa_high, word, offset);
}

} // namespace extractor
