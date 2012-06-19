#include "corpus_tools.h"

#include <iostream>

#include "tdict.h"
#include "filelib.h"
#include "verbose.h"

using namespace std;

void CorpusTools::ReadLine(const string& line,
                           vector<WordID>* src,
                           vector<WordID>* trg) {
  static const WordID kDIV = TD::Convert("|||");
  static vector<WordID> tmp;
  src->clear();
  trg->clear();
  TD::ConvertSentence(line, &tmp);
  unsigned i = 0;
  while(i < tmp.size() && tmp[i] != kDIV) {
    src->push_back(tmp[i]);
    ++i;
  }
  if (i < tmp.size() && tmp[i] == kDIV) {
    ++i;
    for (; i < tmp.size() ; ++i)
      trg->push_back(tmp[i]);
  }
}

void CorpusTools::ReadFromFile(const string& filename,
                           vector<vector<WordID> >* src,
                           set<WordID>* src_vocab,
                           vector<vector<WordID> >* trg,
                           set<WordID>* trg_vocab,
                           int rank,
                           int size) {
  assert(rank >= 0);
  assert(size > 0);
  assert(rank < size);
  if (src) src->clear();
  if (src_vocab) src_vocab->clear();
  if (trg) trg->clear();
  if (trg_vocab) trg_vocab->clear();
  const int expected_fields = 1 + (trg == NULL ? 0 : 1);
  if (!SILENT) cerr << "Reading from " << filename << " ...\n";
  ReadFile rf(filename);
  istream& in = *rf.stream();
  string line;
  int lc = 0;
  static const WordID kDIV = TD::Convert("|||");
  vector<WordID> tmp;
  while(getline(in, line)) {
    const bool skip = (lc % size != rank);
    ++lc;
    TD::ConvertSentence(line, &tmp);
    vector<WordID>* d = NULL;
    if (!skip) {
      src->push_back(vector<WordID>());
      d = &src->back();
    }
    set<WordID>* v = src_vocab;
    int s = 0;
    for (unsigned i = 0; i < tmp.size(); ++i) {
      if (tmp[i] == kDIV) {
        ++s;
        if (s > 1) { cerr << "Unexpected format in line " << lc << ": " << line << endl; abort(); }
        assert(trg);
        if (!skip) {
          trg->push_back(vector<WordID>());
          d = &trg->back();
        }
        v = trg_vocab;
      } else {
        if (d) d->push_back(tmp[i]);
        if (v) v->insert(tmp[i]);
      }
    }
    ++s;
    if (expected_fields != s) {
      cerr << "Wrong number of fields in line " << lc << ": " << line << endl; abort();
    }
  }
}


