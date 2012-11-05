#ifndef _TDICT_H_
#define _TDICT_H_

#include <string>
#include <vector>
#include <cassert>
#include "wordid.h"
#include "dict.h"

struct TD {
  static WordID end(); // next id to be assigned; [begin,end) give the non-reserved tokens seen so far
  static void ConvertSentence(std::string const& sent, std::vector<WordID>* ids);
  static void GetWordIDs(const std::vector<std::string>& strings, std::vector<WordID>* ids);
  static std::string GetString(const std::vector<WordID>& str);
  static std::string GetString(WordID const* i,WordID const* e);
  static int AppendString(const WordID& w, int pos, int bufsize, char* buffer);
  static unsigned int NumWords() {
    return dict_.max();
  }
  static WordID Convert(const std::string& s) {
    return dict_.Convert(s);
  }
  static WordID Convert(char const* s) {
    return dict_.Convert(std::string(s));
  }
  static const std::string& Convert(WordID w) {
    return dict_.Convert(w);
  }
 private:
  static Dict dict_;
};

struct ToTD {
  typedef WordID result_type;
  result_type operator()(std::string const& t) const {
    return TD::Convert(t);
  }
};


#endif
