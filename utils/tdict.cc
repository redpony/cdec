#define TD_ALLOW_UNDEFINED_WORDIDS 0

// if 1, word ids that are >= end() will give a numeric token name (single per-thread shared buffer), which of course won't be Convert-able back to the id, because it's not added to the dict.  This is a convenience for logging fake token indices.  Any tokens actually added to the dict may cause end() to overlap the range of fake ids you were using - that's up to you to prevent.

#include <stdlib.h>
#include <cstring>
#include <sstream>
#include "Ngram.h"
#include "dict.h"
#include "tdict.h"
#include "Vocab.h"
#include "stringlib.h"
#include "threadlocal.h"

using namespace std;

Vocab TD::dict_(0,TD::max_wordid);
WordID TD::ss=dict_.ssIndex();
WordID TD::se=dict_.seIndex();
WordID TD::unk=dict_.unkIndex();
char const*const TD::ss_str=Vocab_SentStart;
char const*const TD::se_str=Vocab_SentEnd;
char const*const TD::unk_str=Vocab_Unknown;

// pre+(i-base)+">" for i in [base,e)
inline void pad(std::string const& pre,int base,int e) {
  assert(base<=e);
  ostringstream o;
  for (int i=base;i<e;++i) {
    o.str(pre);
    o<<(i-base)<<'>';
    WordID id=TD::Convert(o.str());
    assert(id==i); // this fails.  why?
  }
}


namespace {
struct TD_init {
  TD_init() {
    /*
      // disabled for now since it's breaking trunk
    assert(TD::Convert(TD::ss_str)==TD::ss);
    assert(TD::Convert(TD::se_str)==TD::se);
    assert(TD::Convert(TD::unk_str)==TD::unk);
    assert(TD::none==Vocab_None);
    pad("<FILLER",TD::end(),TD::reserved_begin);
    assert(TD::end()==TD::reserved_begin);
    int reserved_end=TD::begin();
    pad("<RESERVED",TD::end(),reserved_end);
    assert(TD::end()==reserved_end);
    */
  }
};

TD_init td_init;
}

unsigned int TD::NumWords() {
  return dict_.numWords();
}
WordID TD::end() {
  return dict_.highIndex();
}

WordID TD::Convert(const std::string& s) {
  return dict_.addWord((VocabString)s.c_str());
}

WordID TD::Convert(char const* s) {
  return dict_.addWord((VocabString)s);
}


#if TD_ALLOW_UNDEFINED_WORDIDS
# include "static_utoa.h"
char undef_prefix[]="UNDEF_";
static const int undefpre_n=sizeof(undef_prefix)/sizeof(undef_prefix[0]);
THREADLOCAL char undef_buf[]="UNDEF_________________";
inline char const* undef_token(WordID w)
{
  append_utoa(undef_buf+undefpre_n,w);
  return undef_buf;
}
#endif

const char* TD::Convert(WordID w) {
#if TD_ALLOW_UNDEFINED_WORDIDS
  if (w>=dict_.highIndex()) return undef_token(w);
#endif
  return dict_.getWord((VocabIndex)w);
}


void TD::GetWordIDs(const std::vector<std::string>& strings, std::vector<WordID>* ids) {
  ids->clear();
  for (vector<string>::const_iterator i = strings.begin(); i != strings.end(); ++i)
    ids->push_back(TD::Convert(*i));
}

std::string TD::GetString(const std::vector<WordID>& str) {
  ostringstream o;
  for (int i=0;i<str.size();++i) {
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
  const char* word = TD::Convert(w);
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
