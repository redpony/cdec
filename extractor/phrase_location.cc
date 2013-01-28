#include "phrase_location.h"

#include <cstdio>

PhraseLocation::PhraseLocation(int sa_low, int sa_high) :
    sa_low(sa_low), sa_high(sa_high),
    matchings(shared_ptr<vector<int> >()),
    num_subpatterns(0) {}

PhraseLocation::PhraseLocation(shared_ptr<vector<int> > matchings,
                               int num_subpatterns) :
    sa_high(0), sa_low(0),
    matchings(matchings),
    num_subpatterns(num_subpatterns) {}

bool PhraseLocation::IsEmpty() {
  return sa_low >= sa_high || (num_subpatterns > 0 && matchings->size() == 0);
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
