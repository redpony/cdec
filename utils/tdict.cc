#define TD_ALLOW_UNDEFINED_WORDIDS 0

// if 1, word ids that are >= end() will give a numeric token name (single per-thread shared buffer), which of course won't be Convert-able back to the id, because it's not added to the dict.  This is a convenience for logging fake token indices.  Any tokens actually added to the dict may cause end() to overlap the range of fake ids you were using - that's up to you to prevent.

#include <stdlib.h>
#include <cstring>
#include <sstream>
#include "dict.h"
#include "tdict.h"
#include "stringlib.h"

using namespace std;

Dict TD::dict_;

void TD::GetWordIDs(const std::vector<std::string>& strings, std::vector<WordID>* ids) {
  ids->clear();
  for (vector<string>::const_iterator i = strings.begin(); i != strings.end(); ++i)
    ids->push_back(TD::Convert(*i));
}

std::string TD::GetString(const std::vector<WordID>& str) {
  ostringstream o;
  for (unsigned i=0;i<str.size();++i) {
    if (i) o << ' ';
    o << TD::Convert(str[i]);
  }
  return o.str();
}

std::string TD::GetString(WordID const* i,WordID const* e) {
  ostringstream o;
  bool sp=false;
  for (;i<e;++i,sp=true) {
    if (sp)
      o << ' ';
    o << TD::Convert(*i);
  }
  return o.str();
}

int TD::AppendString(const WordID& w, int pos, int bufsize, char* buffer)
{
  const string& s = TD::Convert(w);
  const char* word = s.c_str();
  const char* const end_buf = buffer + bufsize;
  char* dest = buffer + pos;
  while(dest < end_buf && *word) {
    *dest = *word;
    ++dest;
    ++word;
  }
  return (dest - buffer);
}


namespace {
struct add_wordids {
  typedef std::vector<WordID> Ws;
  Ws *ids;
  explicit add_wordids(Ws *i) : ids(i) {  }
  add_wordids(const add_wordids& o) : ids(o.ids) {  }
  void operator()(char const* s) {
    ids->push_back(TD::Convert(s));
  }
  void operator()(std::string const& s) {
    ids->push_back(TD::Convert(s));
  }
};

}

void TD::ConvertSentence(std::string const& s, std::vector<WordID>* ids) {
  ids->clear();
  VisitTokens(s,add_wordids(ids));
}
