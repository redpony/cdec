#ifndef _MATCHING_COMPARATOR_H_
#define _MATCHING_COMPARATOR_H_

#include <memory>

using namespace std;

class Vocabulary;
class Matching;

class MatchingComparator {
 public:
  MatchingComparator(int min_gap_size, int max_rule_span);

  int Compare(const Matching& left, const Matching& right,
              int last_chunk_len, bool offset) const;

 private:
  int min_gap_size;
  int max_rule_span;
};

#endif
