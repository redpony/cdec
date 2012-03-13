#include "corpus.h"

#include <set>
#include <vector>
#include <string>

#include "tdict.h"
#include "filelib.h"

using namespace std;

namespace corpus {

void ReadParallelCorpus(const string& filename,
                vector<vector<WordID> >* f,
                vector<vector<WordID> >* e,
                set<WordID>* vocab_f,
                set<WordID>* vocab_e) {
  f->clear();
  e->clear();
  vocab_f->clear();
  vocab_e->clear();
  ReadFile rf(filename);
  istream* in = rf.stream();
  assert(*in);
  string line;
  unsigned lc = 0;
  const WordID kDIV = TD::Convert("|||");
  vector<WordID> tmp;
  while(getline(*in, line)) {
    ++lc;
    e->push_back(vector<int>());
    f->push_back(vector<int>());
    vector<int>& le = e->back();
    vector<int>& lf = f->back();
    tmp.clear();
    TD::ConvertSentence(line, &tmp);
    bool isf = true;
    for (unsigned i = 0; i < tmp.size(); ++i) {
      const int cur = tmp[i];
      if (isf) {
        if (kDIV == cur) {
          isf = false;
        } else {
          lf.push_back(cur);
          vocab_f->insert(cur);
        }
      } else {
        if (cur == kDIV) {
          cerr << "ERROR in " << lc << ": " << line << endl << endl;
          abort();
        }
        le.push_back(cur);
        vocab_e->insert(cur);
      }
    }
    assert(isf == false);
  }
}

}

