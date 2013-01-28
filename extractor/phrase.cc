#include "phrase.h"

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
