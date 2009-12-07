#ifndef _FREQDICT_H_
#define _FREQDICT_H_

#include <map>
#include <string>
#include "wordid.h"

class FreqDict {
 public:
  void Load(const std::string& fname);
  float LookUp(const WordID& word) const {
    std::map<WordID,float>::const_iterator i = counts_.find(word);
    if (i == counts_.end()) return 0;
    return i->second;
  }
 private:
  std::map<WordID, float> counts_;
};

#endif
