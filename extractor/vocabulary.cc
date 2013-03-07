#include "vocabulary.h"

namespace extractor {

Vocabulary::~Vocabulary() {}

int Vocabulary::GetTerminalIndex(const string& word) {
  int word_id = -1;
  #pragma omp critical (vocabulary)
  {
    if (!dictionary.count(word)) {
      word_id = words.size();
      dictionary[word] = word_id;
      words.push_back(word);
    } else {
      word_id = dictionary[word];
    }
  }
  return word_id;
}

int Vocabulary::GetNonterminalIndex(int position) {
  return -position;
}

bool Vocabulary::IsTerminal(int symbol) {
  return symbol >= 0;
}

string Vocabulary::GetTerminalValue(int symbol) {
  string word;
  #pragma omp critical (vocabulary)
  word = words[symbol];
  return word;
}

} // namespace extractor
