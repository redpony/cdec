#include "Ngram.h"
#include "dict.h"
#include "tdict.h"
#include "Vocab.h"

using namespace std;

Vocab* TD::dict_ = new Vocab;

static const string empty;
static const string space = " ";

WordID TD::Convert(const std::string& s) {
  return dict_->addWord((VocabString)s.c_str());
}

const char* TD::Convert(const WordID& w) {
  return dict_->getWord((VocabIndex)w);
}

void TD::GetWordIDs(const std::vector<std::string>& strings, std::vector<WordID>* ids) {
  ids->clear();
  for (vector<string>::const_iterator i = strings.begin(); i != strings.end(); ++i)
    ids->push_back(TD::Convert(*i));
}

std::string TD::GetString(const std::vector<WordID>& str) {
  string res;
  for (vector<WordID>::const_iterator i = str.begin(); i != str.end(); ++i)
    res += (i == str.begin() ? empty : space) + TD::Convert(*i);
  return res;
}

void TD::ConvertSentence(const std::string& sent, std::vector<WordID>* ids) {
  string s = sent;
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

