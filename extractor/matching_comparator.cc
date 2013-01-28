#include "matching_comparator.h"

#include "matching.h"
#include "vocabulary.h"

MatchingComparator::MatchingComparator(int min_gap_size, int max_rule_span) :
    min_gap_size(min_gap_size), max_rule_span(max_rule_span) {}

int MatchingComparator::Compare(const Matching& left,
                                const Matching& right,
                                int last_chunk_len,
                                bool offset) const {
  if (left.sentence_id != right.sentence_id) {
    return left.sentence_id < right.sentence_id ? -1 : 1;
  }

  if (left.positions.size() == 1 && right.positions.size() == 1) {
    // The prefix and the suffix must be separated by a non-terminal, otherwise
    // we would be doing a suffix array lookup.
    if (right.positions[0] - left.positions[0] <= min_gap_size) {
      return 1;
    }
  } else if (offset) {
    for (size_t i = 1; i < left.positions.size(); ++i) {
      if (left.positions[i] != right.positions[i - 1]) {
        return left.positions[i] < right.positions[i - 1] ? -1 : 1;
      }
    }
  } else {
    if (left.positions[0] + 1 != right.positions[0]) {
      return left.positions[0] + 1 < right.positions[0] ? -1 : 1;
    }
    for (size_t i = 1; i < left.positions.size(); ++i) {
      if (left.positions[i] != right.positions[i]) {
        return left.positions[i] < right.positions[i] ? -1 : 1;
      }
    }
  }

  if (right.positions.back() + last_chunk_len - left.positions.front() >
      max_rule_span) {
    return -1;
  }

  return 0;
}
