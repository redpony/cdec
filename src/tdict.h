#ifndef _TDICT_H_
#define _TDICT_H_

#include <string>
#include <vector>
#include "wordid.h"

class Vocab;

struct TD {
  static Vocab* dict_;
  static void ConvertSentence(const std::string& sent, std::vector<WordID>* ids);
  static void GetWordIDs(const std::vector<std::string>& strings, std::vector<WordID>* ids);
  static std::string GetString(const std::vector<WordID>& str);
  static WordID Convert(const std::string& s);
  static const char* Convert(const WordID& w);
};

#endif
