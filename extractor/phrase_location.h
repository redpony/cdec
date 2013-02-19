#ifndef _PHRASE_LOCATION_H_
#define _PHRASE_LOCATION_H_

#include <memory>
#include <vector>

using namespace std;

struct PhraseLocation {
  PhraseLocation(int sa_low = -1, int sa_high = -1);

  PhraseLocation(const vector<int>& matchings, int num_subpatterns);

  bool IsEmpty() const;

  int GetSize() const;

  friend bool operator==(const PhraseLocation& a, const PhraseLocation& b);

  int sa_low, sa_high;
  shared_ptr<vector<int> > matchings;
  int num_subpatterns;
};

#endif
