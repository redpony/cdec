#ifndef _TDICT_H_
#define _TDICT_H_

#include <string>
#include <vector>
#include "wordid.h"
#include "dict.h"

class Vocab;

struct TD {

  static Dict dict_;
  static std::string empty;
  static std::string space;

  static std::string GetString(const std::vector<WordID>& str) {
    std::string res;
    for (std::vector<WordID>::const_iterator i = str.begin(); i != str.end(); ++i)
      res += (i == str.begin() ? empty : space) + TD::Convert(*i);
    return res;
  }

  static void ConvertSentence(const std::string& sent, std::vector<WordID>* ids) {
    std::string s = sent;
    int last = 0;
    ids->clear();
    for (int i=0; i < s.size(); ++i)
      if (s[i] == 32 || s[i] == '\t') {
        s[i]=0;
        if (last != i) {
          ids->push_back(Convert(&s[last]));
        }
        last = i + 1;
      }
    if (last != s.size())
      ids->push_back(Convert(&s[last]));
  }

  static WordID Convert(const std::string& s) {
    return dict_.Convert(s);
  }

  static const std::string& Convert(const WordID& w) {
    return dict_.Convert(w);
  }
};

#endif
