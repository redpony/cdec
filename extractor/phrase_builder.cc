#include "phrase_builder.h"

#include "phrase.h"
#include "vocabulary.h"

namespace extractor {

PhraseBuilder::PhraseBuilder(shared_ptr<Vocabulary> vocabulary) :
    vocabulary(vocabulary) {}

Phrase PhraseBuilder::Build(const vector<int>& symbols) {
  Phrase phrase;
  phrase.symbols = symbols;
  for (size_t i = 0; i < symbols.size(); ++i) {
    if (vocabulary->IsTerminal(symbols[i])) {
      phrase.words.push_back(vocabulary->GetTerminalValue(symbols[i]));
    } else {
      phrase.var_pos.push_back(i);
    }
  }
  return phrase;
}

Phrase PhraseBuilder::Extend(const Phrase& phrase, bool start_x, bool end_x) {
  vector<int> symbols = phrase.Get();
  int num_nonterminals = 0;
  if (start_x) {
    num_nonterminals = 1;
    symbols.insert(symbols.begin(),
        vocabulary->GetNonterminalIndex(num_nonterminals));
  }

  for (size_t i = start_x; i < symbols.size(); ++i) {
    if (!vocabulary->IsTerminal(symbols[i])) {
      ++num_nonterminals;
      symbols[i] = vocabulary->GetNonterminalIndex(num_nonterminals);
    }
  }

  if (end_x) {
    ++num_nonterminals;
    symbols.push_back(vocabulary->GetNonterminalIndex(num_nonterminals));
  }

  return Build(symbols);
}

} // namespace extractor
