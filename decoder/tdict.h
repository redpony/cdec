#ifndef _TDICT_H_
#define _TDICT_H_

#include <string>
#include <vector>
#include "wordid.h"
#include <assert.h>

class Vocab;

struct TD {
  /* // disabled for now
  static const int reserved_begin=10; // allow room for SRI special tokens e.g. unk ss se pause.  tokens until this get "<FILLERi>"
  static const int n_reserved=10; // 0...n_reserved-1 get token '<RESERVEDi>'
  static inline WordID reserved(int i) {
    assert(i>=0 && i<n_reserved);
    return (WordID)(reserved_begin+i);
  }
  static inline WordID begin() {
    return reserved(n_reserved);
  }
  */
  static const WordID max_wordid=0x7fffffff;
  static const WordID none=(WordID)-1; // Vocab_None
  static char const* const ss_str;  //="<s>";
  static char const* const se_str;  //="</s>";
  static char const* const unk_str; //="<unk>";
  static WordID ss,se,unk; // x=Convert(x_str)
  static WordID end(); // next id to be assigned; [begin,end) give the non-reserved tokens seen so far
  static Vocab dict_;
  static void ConvertSentence(std::string const& sent, std::vector<WordID>* ids);
  static void GetWordIDs(const std::vector<std::string>& strings, std::vector<WordID>* ids);
  static std::string GetString(const std::vector<WordID>& str);
  static std::string GetString(WordID const* i,WordID const* e);
  static int AppendString(const WordID& w, int pos, int bufsize, char* buffer);
  static unsigned int NumWords();
  static WordID Convert(const std::string& s);
  static WordID Convert(char const* s);
  static const char* Convert(const WordID& w);
};

struct ToTD {
  typedef WordID result_type;
  result_type operator()(std::string const& t) const {
    return TD::Convert(t);
  }
};


#endif
