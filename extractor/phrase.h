#ifndef _PHRASE_H_
#define _PHRASE_H_

#include <iostream>
#include <string>
#include <vector>

#include "phrase_builder.h"

using namespace std;

namespace extractor {

class Phrase {
 public:
  friend Phrase PhraseBuilder::Build(const vector<int>& phrase);

  int Arity() const;

  int GetChunkLen(int index) const;

  vector<int> Get() const;

  int GetSymbol(int position) const;

  //TODO(pauldb): Unit test this method.
  int GetNumSymbols() const;

  //TODO(pauldb): Add unit tests.
  vector<string> GetWords() const;

  //TODO(pauldb): Add unit tests.
  int operator<(const Phrase& other) const;

  friend ostream& operator<<(ostream& os, const Phrase& phrase);

 private:
  vector<int> symbols;
  vector<int> var_pos;
  vector<string> words;
};

} // namespace extractor

#endif
