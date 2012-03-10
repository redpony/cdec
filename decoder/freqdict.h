#ifndef _FREQDICT_H_
#define _FREQDICT_H_

#include <iostream>
#include <map>
#include <string>
#include "wordid.h"
#include "filelib.h"
#include "tdict.h"

template <typename T = float>
class FreqDict {
 public:
  FreqDict() : max_() {}
  T Max() const { return max_; }
  void Load(const std::string& fname) {
    std::cerr << "Reading word statistics from: " << fname << std::endl;
    ReadFile rf(fname);
    std::istream& ifs = *rf.stream();
    int cc=0;
    std::string word;
    while (ifs) {
      ifs >> word;
      if (word.size() == 0) continue;
      if (word[0] == '#') continue;
      T count = 0;
      ifs >> count;
      if (count > max_) max_ = count;
      counts_[TD::Convert(word)]=count;
      ++cc;
      if (cc % 10000 == 0) { std::cerr << "."; }
    }
    std::cerr << "\n";
    std::cerr << "Loaded " << cc << " words\n";
  }

  T LookUp(const WordID& word) const {
    typename std::map<WordID,T>::const_iterator i = counts_.find(word);
    if (i == counts_.end()) return T();
    return i->second;
  }
 private:
  T max_;
  std::map<WordID, T> counts_;
};

#endif
