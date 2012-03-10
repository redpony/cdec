#ifndef _OS_PHRASE_H_
#define _OS_PHRASE_H_

#include <iostream>
#include <vector>
#include "tdict.h"

inline std::ostream& operator<<(std::ostream& os, const std::vector<WordID>& p) {
  os << '[';
  for (int i = 0; i < p.size(); ++i)
    os << (i==0 ? "" : " ") << TD::Convert(p[i]);
  return os << ']';
}

#endif
