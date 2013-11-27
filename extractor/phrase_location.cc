#include "phrase_location.h"

#include <iostream>

namespace extractor {

PhraseLocation::PhraseLocation(int sa_low, int sa_high) :
    sa_low(sa_low), sa_high(sa_high), num_subpatterns(0) {}

PhraseLocation::PhraseLocation(const vector<int>& matchings,
                               int num_subpatterns) :
    sa_low(0), sa_high(0),
    matchings(make_shared<vector<int>>(matchings)),
    num_subpatterns(num_subpatterns) {}

bool PhraseLocation::IsEmpty() const {
  return GetSize() == 0;
}

int PhraseLocation::GetSize() const {
  if (num_subpatterns > 0) {
    return matchings->size();
  } else {
    return sa_high - sa_low;
  }
}

bool operator==(const PhraseLocation& a, const PhraseLocation& b) {
  if (a.sa_low != b.sa_low || a.sa_high != b.sa_high ||
      a.num_subpatterns != b.num_subpatterns) {
    return false;
  }

  if (a.matchings == NULL && b.matchings == NULL) {
    return true;
  }

  if (a.matchings == NULL || b.matchings == NULL) {
    return false;
  }

  return *a.matchings == *b.matchings;
}

} // namespace extractor
