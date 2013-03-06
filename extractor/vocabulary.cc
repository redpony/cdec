#include "vocabulary.h"

namespace extractor {

Vocabulary::~Vocabulary() {}

int Vocabulary::GetTerminalIndex(const string& word) {
  if (!dictionary.count(word)) {
    int word_id = words.size();
    dictionary[word] = word_id;
    words.push_back(word);
    return word_id;
  }

  return dictionary[word];
}

int Vocabulary::GetNonterminalIndex(int position) {
  return -position;
}

bool Vocabulary::IsTerminal(int symbol) {
  return symbol >= 0;
}

string Vocabulary::GetTerminalValue(int symbol) {
  return words[symbol];
}

int Vocabulary::Size() {
  return words.size();
}

} // namespace extractor
