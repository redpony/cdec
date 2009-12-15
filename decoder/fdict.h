#ifndef _FDICT_H_
#define _FDICT_H_

#include <string>
#include <vector>
#include "dict.h"

struct FD {
  static Dict dict_;
  static inline int NumFeats() {
    return dict_.max() + 1;
  }
  static inline WordID Convert(const std::string& s) {
    return dict_.Convert(s);
  }
  static inline const std::string& Convert(const WordID& w) {
    return dict_.Convert(w);
  }
};

#endif
