#include "dict.h"

#include <string>
#include <vector>

void TokenizeStringSeparator(
          const std::string& str,
          const std::string& separator,
          std::vector<std::string>* tokens) {

  size_t pos = 0;
  std::string::size_type nextPos = str.find(separator, pos);

  while (nextPos != std::string::npos) {
    tokens->push_back(str.substr(pos, nextPos - pos));
    pos = nextPos + separator.size();
    nextPos = str.find(separator, pos);
  }
  tokens->push_back(str.substr(pos, nextPos - pos));
}


void Dict::AsVector(const WordID& id, std::vector<std::string>* results) const {
  results->clear();
  TokenizeStringSeparator(Convert(id), " ||| ", results);
}

