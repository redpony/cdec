#include "binary_search_merger.h"

#include "data_array.h"
#include "linear_merger.h"
#include "matching.h"
#include "matching_comparator.h"
#include "phrase.h"
#include "vocabulary.h"

double BinarySearchMerger::BAEZA_YATES_FACTOR = 1.0;

BinarySearchMerger::BinarySearchMerger(
    shared_ptr<Vocabulary> vocabulary,
    shared_ptr<LinearMerger> linear_merger,
    shared_ptr<DataArray> data_array,
    shared_ptr<MatchingComparator> comparator,
    bool force_binary_search_merge) :
    vocabulary(vocabulary), linear_merger(linear_merger),
    data_array(data_array), comparator(comparator),
    force_binary_search_merge(force_binary_search_merge) {}

BinarySearchMerger::BinarySearchMerger() {}

BinarySearchMerger::~BinarySearchMerger() {}

void BinarySearchMerger::Merge(
    vector<int>& locations, const Phrase& phrase, const Phrase& suffix,
    vector<int>::iterator prefix_start, vector<int>::iterator prefix_end,
    vector<int>::iterator suffix_start, vector<int>::iterator suffix_end,
    int prefix_subpatterns, int suffix_subpatterns) const {
  if (IsIntersectionVoid(prefix_start, prefix_end, suffix_start, suffix_end,
      prefix_subpatterns, suffix_subpatterns, suffix)) {
    return;
  }

  int prefix_set_size = prefix_end - prefix_start;
  int suffix_set_size = suffix_end - suffix_start;
  if (ShouldUseLinearMerge(prefix_set_size, suffix_set_size)) {
    linear_merger->Merge(locations, phrase, suffix, prefix_start, prefix_end,
          suffix_start, suffix_end, prefix_subpatterns, suffix_subpatterns);
    return;
  }

  vector<int>::iterator low, high, prefix_low, prefix_high, suffix_mid;
  if (prefix_set_size > suffix_set_size) {
    // Binary search on the prefix set.
    suffix_mid = GetMiddle(suffix_start, suffix_end, suffix_subpatterns);
    low = prefix_start, high = prefix_end;
    while (low < high) {
      vector<int>::iterator prefix_mid =
          GetMiddle(low, high, prefix_subpatterns);

      GetComparableMatchings(prefix_start, prefix_end, prefix_mid,
          prefix_subpatterns, prefix_low, prefix_high);
      int comparison = CompareMatchingsSet(prefix_low, prefix_high, suffix_mid,
          prefix_subpatterns, suffix_subpatterns, suffix);
      if (comparison == 0) {
        break;
      } else if (comparison < 0) {
        low = prefix_mid + prefix_subpatterns;
      } else {
        high = prefix_mid;
      }
    }
  } else {
    // Binary search on the suffix set.
    vector<int>::iterator prefix_mid =
        GetMiddle(prefix_start, prefix_end, prefix_subpatterns);

    GetComparableMatchings(prefix_start, prefix_end, prefix_mid,
        prefix_subpatterns, prefix_low, prefix_high);
    low = suffix_start, high = suffix_end;
    while (low < high) {
      suffix_mid = GetMiddle(low, high, suffix_subpatterns);

      int comparison = CompareMatchingsSet(prefix_low, prefix_high, suffix_mid,
          prefix_subpatterns, suffix_subpatterns, suffix);
      if (comparison == 0) {
        break;
      } else if (comparison > 0) {
        low = suffix_mid + suffix_subpatterns;
      } else {
        high = suffix_mid;
      }
    }
  }

  vector<int> result;
  int last_chunk_len = suffix.GetChunkLen(suffix.Arity());
  bool offset = !vocabulary->IsTerminal(suffix.GetSymbol(0));
  vector<int>::iterator suffix_low, suffix_high;
  if (low < high) {
    // We found a group of prefixes with the same starting position that give
    // different results when compared to the found suffix.
    // Find all matching suffixes for the previously found set of prefixes.
    suffix_low = suffix_mid;
    suffix_high = suffix_mid + suffix_subpatterns;
    for (auto i = prefix_low; i != prefix_high; i += prefix_subpatterns) {
      Matching left(i, prefix_subpatterns, data_array->GetSentenceId(*i));
      while (suffix_low != suffix_start) {
        Matching right(suffix_low - suffix_subpatterns, suffix_subpatterns,
            data_array->GetSentenceId(*(suffix_low - suffix_subpatterns)));
        if (comparator->Compare(left, right, last_chunk_len, offset) <= 0) {
          suffix_low -= suffix_subpatterns;
        } else {
          break;
        }
      }

      for (auto j = suffix_low; j != suffix_end; j += suffix_subpatterns) {
        Matching right(j, suffix_subpatterns, data_array->GetSentenceId(*j));
        int comparison = comparator->Compare(left, right, last_chunk_len,
                                             offset);
        if (comparison == 0) {
          vector<int> merged = left.Merge(right, phrase.Arity() + 1);
          result.insert(result.end(), merged.begin(), merged.end());
        } else if (comparison < 0) {
          break;
        }
        suffix_high = max(suffix_high, j + suffix_subpatterns);
      }
    }

    swap(suffix_low, suffix_high);
  } else if (prefix_set_size > suffix_set_size) {
    // We did the binary search on the prefix set.
    suffix_low = suffix_mid;
    suffix_high = suffix_mid + suffix_subpatterns;
    if (CompareMatchingsSet(prefix_low, prefix_high, suffix_mid,
        prefix_subpatterns, suffix_subpatterns, suffix) < 0) {
      prefix_low = prefix_high;
    } else {
      prefix_high = prefix_low;
    }
  } else {
    // We did the binary search on the suffix set.
    if (CompareMatchingsSet(prefix_low, prefix_high, suffix_mid,
        prefix_subpatterns, suffix_subpatterns, suffix) < 0) {
      suffix_low = suffix_mid;
      suffix_high = suffix_mid;
    } else {
      suffix_low = suffix_mid + suffix_subpatterns;
      suffix_high = suffix_mid + suffix_subpatterns;
    }
  }

  Merge(locations, phrase, suffix, prefix_start, prefix_low, suffix_start,
        suffix_low, prefix_subpatterns, suffix_subpatterns);
  locations.insert(locations.end(), result.begin(), result.end());
  Merge(locations, phrase, suffix, prefix_high, prefix_end, suffix_high,
        suffix_end, prefix_subpatterns, suffix_subpatterns);
}

bool BinarySearchMerger::IsIntersectionVoid(
    vector<int>::iterator prefix_start, vector<int>::iterator prefix_end,
    vector<int>::iterator suffix_start, vector<int>::iterator suffix_end,
    int prefix_subpatterns, int suffix_subpatterns,
    const Phrase& suffix) const {
  // Is any of the sets empty?
  if (prefix_start >= prefix_end || suffix_start >= suffix_end) {
    return true;
  }

  int last_chunk_len = suffix.GetChunkLen(suffix.Arity());
  bool offset = !vocabulary->IsTerminal(suffix.GetSymbol(0));
  // Is the first value from the first set larger than the last value in the
  // second set?
  Matching left(prefix_start, prefix_subpatterns,
      data_array->GetSentenceId(*prefix_start));
  Matching right(suffix_end - suffix_subpatterns, suffix_subpatterns,
      data_array->GetSentenceId(*(suffix_end - suffix_subpatterns)));
  if (comparator->Compare(left, right, last_chunk_len, offset) > 0) {
    return true;
  }

  // Is the last value from the first set smaller than the first value in the
  // second set?
  left = Matching(prefix_end - prefix_subpatterns, prefix_subpatterns,
      data_array->GetSentenceId(*(prefix_end - prefix_subpatterns)));
  right = Matching(suffix_start, suffix_subpatterns,
      data_array->GetSentenceId(*suffix_start));
  if (comparator->Compare(left, right, last_chunk_len, offset) < 0) {
    return true;
  }

  return false;
}

bool BinarySearchMerger::ShouldUseLinearMerge(
    int prefix_set_size, int suffix_set_size) const {
  if (force_binary_search_merge) {
    return false;
  }

  int min_size = min(prefix_set_size, suffix_set_size);
  int max_size = max(prefix_set_size, suffix_set_size);

  return BAEZA_YATES_FACTOR * min_size * log2(max_size) > max_size;
}

vector<int>::iterator BinarySearchMerger::GetMiddle(
    vector<int>::iterator low, vector<int>::iterator high,
    int num_subpatterns) const {
  return low + (((high - low) / num_subpatterns) / 2) * num_subpatterns;
}

void BinarySearchMerger::GetComparableMatchings(
    const vector<int>::iterator& prefix_start,
    const vector<int>::iterator& prefix_end,
    const vector<int>::iterator& prefix_mid,
    int num_subpatterns,
    vector<int>::iterator& prefix_low,
    vector<int>::iterator& prefix_high) const {
  prefix_low = prefix_mid;
  while (prefix_low != prefix_start
      && *prefix_mid == *(prefix_low - num_subpatterns)) {
    prefix_low -= num_subpatterns;
  }
  prefix_high = prefix_mid + num_subpatterns;
  while (prefix_high != prefix_end
      && *prefix_mid == *prefix_high) {
    prefix_high += num_subpatterns;
  }
}

int BinarySearchMerger::CompareMatchingsSet(
    const vector<int>::iterator& prefix_start,
    const vector<int>::iterator& prefix_end,
    const vector<int>::iterator& suffix_mid,
    int prefix_subpatterns,
    int suffix_subpatterns,
    const Phrase& suffix) const {
  int result = 0;
  int last_chunk_len = suffix.GetChunkLen(suffix.Arity());
  bool offset = !vocabulary->IsTerminal(suffix.GetSymbol(0));

  Matching right(suffix_mid, suffix_subpatterns,
      data_array->GetSentenceId(*suffix_mid));
  for (auto i = prefix_start; i != prefix_end; i += prefix_subpatterns) {
    Matching left(i, prefix_subpatterns, data_array->GetSentenceId(*i));
    int comparison = comparator->Compare(left, right, last_chunk_len, offset);
    if (i == prefix_start) {
      result = comparison;
    } else if (comparison != result) {
      return 0;
    }
  }
  return result;
}
