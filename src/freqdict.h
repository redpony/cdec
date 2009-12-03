#ifndef _FREQDICT_H_
#define _FREQDICT_H_

#include <map>
#include <string>

class FreqDict {
 public:
  void load(const std::string& fname);
  float frequency(const std::string& word) const {
    std::map<std::string,float>::const_iterator i = counts_.find(word);
    if (i == counts_.end()) return 0;
    return i->second;
  }
 private:
  std::map<std::string, float> counts_;
};

#endif
