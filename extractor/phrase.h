#ifndef _PHRASE_H_
#define _PHRASE_H_

#include <string>
#include <vector>

#include "phrase_builder.h"

using namespace std;

class Phrase {
 public:
  friend Phrase PhraseBuilder::Build(const vector<int>& phrase);

  int Arity() const;

  int GetChunkLen(int index) const;

  vector<int> Get() const;

  int GetSymbol(int position) const;

 private:
  vector<int> symbols;
  vector<int> var_pos;
  vector<string> words;
};

#endif
