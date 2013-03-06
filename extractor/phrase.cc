#include "phrase.h"

namespace extractor {

int Phrase::Arity() const {
  return var_pos.size();
}

int Phrase::GetChunkLen(int index) const {
  if (var_pos.size() == 0) {
    return symbols.size();
  } else if (index == 0) {
    return var_pos[0];
  } else if (index == var_pos.size()) {
    return symbols.size() - var_pos.back() - 1;
  } else {
    return var_pos[index] - var_pos[index - 1] - 1;
  }
}

vector<int> Phrase::Get() const {
  return symbols;
}

int Phrase::GetSymbol(int position) const {
  return symbols[position];
}

int Phrase::GetNumSymbols() const {
  return symbols.size();
}

vector<string> Phrase::GetWords() const {
  return words;
}

bool Phrase::operator<(const Phrase& other) const {
  return symbols < other.symbols;
}

ostream& operator<<(ostream& os, const Phrase& phrase) {
  int current_word = 0;
  for (size_t i = 0; i < phrase.symbols.size(); ++i) {
    if (phrase.symbols[i] < 0) {
      os << "[X," << -phrase.symbols[i] << "]";
    } else {
      os << phrase.words[current_word];
      ++current_word;
    }

    if (i + 1 < phrase.symbols.size()) {
      os << " ";
    }
  }
  return os;
}

} // namspace extractor
