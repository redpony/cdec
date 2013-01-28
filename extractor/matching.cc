#include "matching.h"

Matching::Matching(vector<int>::iterator start, int len, int sentence_id) :
    positions(start, start + len), sentence_id(sentence_id) {}

vector<int> Matching::Merge(const Matching& other, int num_subpatterns) const {
  vector<int> result = positions;
  if (num_subpatterns > positions.size()) {
    result.push_back(other.positions.back());
  }
  return result;
}
