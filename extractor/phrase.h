#ifndef _PHRASE_H_
#define _PHRASE_H_

#include <iostream>
#include <string>
#include <vector>

#include "phrase_builder.h"

using namespace std;

namespace extractor {

/**
 * Structure containing the data for a phrase.
 */
class Phrase {
 public:
  friend Phrase PhraseBuilder::Build(const vector<int>& phrase);

  // Returns the number of nonterminals in the phrase.
  int Arity() const;

  // Returns the number of terminals (length) for the given chunk. (A chunk is a
  // contiguous sequence of terminals in the phrase).
  int GetChunkLen(int index) const;

  // Returns the symbols (word ids) marking up the phrase.
  vector<int> Get() const;

  // Returns the symbol located at the given position in the phrase.
  int GetSymbol(int position) const;

  // Returns the number of symbols in the phrase.
  int GetNumSymbols() const;

  // Returns the words making up the phrase. (Nonterminals are stripped out.)
  vector<string> GetWords() const;

  bool operator<(const Phrase& other) const;

  friend ostream& operator<<(ostream& os, const Phrase& phrase);

 private:
  vector<int> symbols;
  vector<int> var_pos;
  vector<string> words;
};

} // namespace extractor

#endif
