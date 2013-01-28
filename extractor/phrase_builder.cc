#include "phrase_builder.h"

#include "phrase.h"
#include "vocabulary.h"

PhraseBuilder::PhraseBuilder(shared_ptr<Vocabulary> vocabulary) :
    vocabulary(vocabulary) {}

Phrase PhraseBuilder::Build(const vector<int>& symbols) {
  Phrase phrase;
  phrase.symbols = symbols;
  phrase.words.resize(symbols.size());
  for (size_t i = 0; i < symbols.size(); ++i) {
    if (vocabulary->IsTerminal(symbols[i])) {
      phrase.words[i] = vocabulary->GetTerminalValue(symbols[i]);
    } else {
      phrase.var_pos.push_back(i);
    }
  }
  return phrase;
}
