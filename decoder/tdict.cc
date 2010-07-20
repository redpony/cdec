#include <sstream>
#include "Ngram.h"
#include "dict.h"
#include "tdict.h"
#include "Vocab.h"
#include "stringlib.h"

using namespace std;

//FIXME: valgrind errors (static init order?)
Vocab TD::dict_;

unsigned int TD::NumWords() {
  return dict_.numWords();
}

WordID TD::Convert(const std::string& s) {
  return dict_.addWord((VocabString)s.c_str());
}

WordID TD::Convert(char const* s) {
  return dict_.addWord((VocabString)s);
}

const char* TD::Convert(const WordID& w) {
  return dict_.getWord((VocabIndex)w);
}

static const string empty;
static const string space = " ";


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
