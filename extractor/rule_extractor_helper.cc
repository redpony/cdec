#include "rule_extractor_helper.h"

#include "data_array.h"
#include "alignment.h"

namespace extractor {

RuleExtractorHelper::RuleExtractorHelper(
    shared_ptr<DataArray> source_data_array,
    shared_ptr<DataArray> target_data_array,
    shared_ptr<Alignment> alignment,
    int max_rule_span,
    int max_rule_symbols,
    bool require_aligned_terminal,
    bool require_aligned_chunks,
    bool require_tight_phrases) :
    source_data_array(source_data_array),
    target_data_array(target_data_array),
    alignment(alignment),
    max_rule_span(max_rule_span),
    max_rule_symbols(max_rule_symbols),
    require_aligned_terminal(require_aligned_terminal),
    require_aligned_chunks(require_aligned_chunks),
    require_tight_phrases(require_tight_phrases) {}

RuleExtractorHelper::RuleExtractorHelper() {}

RuleExtractorHelper::~RuleExtractorHelper() {}

void RuleExtractorHelper::GetLinksSpans(
    vector<int>& source_low, vector<int>& source_high,
    vector<int>& target_low, vector<int>& target_high, int sentence_id) const {
  int source_sent_len = source_data_array->GetSentenceLength(sentence_id);
  int target_sent_len = target_data_array->GetSentenceLength(sentence_id);
  source_low = vector<int>(source_sent_len, -1);
  source_high = vector<int>(source_sent_len, -1);

  target_low = vector<int>(target_sent_len, -1);
  target_high = vector<int>(target_sent_len, -1);
  vector<pair<int, int> > links = alignment->GetLinks(sentence_id);
  for (auto link: links) {
    if (source_low[link.first] == -1 || source_low[link.first] > link.second) {
      source_low[link.first] = link.second;
    }
    source_high[link.first] = max(source_high[link.first], link.second + 1);

    if (target_low[link.second] == -1 || target_low[link.second] > link.first) {
      target_low[link.second] = link.first;
    }
    target_high[link.second] = max(target_high[link.second], link.first + 1);
  }
}

bool RuleExtractorHelper::CheckAlignedTerminals(
    const vector<int>& matching,
    const vector<int>& chunklen,
    const vector<int>& source_low,
    int source_sent_start) const {
  if (!require_aligned_terminal) {
    return true;
  }

  int num_aligned_chunks = 0;
  for (size_t i = 0; i < chunklen.size(); ++i) {
    for (size_t j = 0; j < chunklen[i]; ++j) {
      int sent_index = matching[i] - source_sent_start + j;
      if (source_low[sent_index] != -1) {
        ++num_aligned_chunks;
        break;
      }
    }
  }

  if (num_aligned_chunks == 0) {
    return false;
  }

  return !require_aligned_chunks || num_aligned_chunks == chunklen.size();
}

bool RuleExtractorHelper::CheckTightPhrases(
    const vector<int>& matching,
    const vector<int>& chunklen,
    const vector<int>& source_low,
    int source_sent_start) const {
  if (!require_tight_phrases) {
    return true;
  }

  // Check if the chunk extremities are aligned.
  for (size_t i = 0; i + 1 < chunklen.size(); ++i) {
    int gap_start = matching[i] + chunklen[i] - source_sent_start;
    int gap_end = matching[i + 1] - 1 - source_sent_start;
    if (source_low[gap_start] == -1 || source_low[gap_end] == -1) {
      return false;
    }
  }

  return true;
}

bool RuleExtractorHelper::FindFixPoint(
    int source_phrase_low, int source_phrase_high,
    const vector<int>& source_low, const vector<int>& source_high,
    int& target_phrase_low, int& target_phrase_high,
    const vector<int>& target_low, const vector<int>& target_high,
    int& source_back_low, int& source_back_high, int sentence_id,
    int min_source_gap_size, int min_target_gap_size,
    int max_new_x, bool allow_low_x, bool allow_high_x,
    bool allow_arbitrary_expansion) const {
  int prev_target_low = target_phrase_low;
  int prev_target_high = target_phrase_high;

  FindProjection(source_phrase_low, source_phrase_high, source_low,
                 source_high, target_phrase_low, target_phrase_high);

  if (target_phrase_low == -1) {
    // Note: Low priority corner case inherited from Adam's code:
    // If w is unaligned, but we don't require aligned terminals, returning an
    // error here prevents the extraction of the allowed rule
    // X -> X_1 w X_2 / X_1 X_2
    return false;
  }

  int source_sent_len = source_data_array->GetSentenceLength(sentence_id);
  int target_sent_len = target_data_array->GetSentenceLength(sentence_id);
  // Extend the target span to the left.
  if (prev_target_low != -1 && target_phrase_low != prev_target_low) {
    if (prev_target_low - target_phrase_low < min_target_gap_size) {
      target_phrase_low = prev_target_low - min_target_gap_size;
      if (target_phrase_low < 0) {
        return false;
      }
    }
  }

  // Extend the target span to the right.
  if (prev_target_high != -1 && target_phrase_high != prev_target_high) {
    if (target_phrase_high - prev_target_high < min_target_gap_size) {
      target_phrase_high = prev_target_high + min_target_gap_size;
      if (target_phrase_high > target_sent_len) {
        return false;
      }
    }
  }

  // Check target span length.
  if (target_phrase_high - target_phrase_low > max_rule_span) {
    return false;
  }

  // Find the initial reflected source span.
  source_back_low = source_back_high = -1;
  FindProjection(target_phrase_low, target_phrase_high, target_low, target_high,
                 source_back_low, source_back_high);
  int new_x = 0;
  bool new_low_x = false, new_high_x = false;
  while (true) {
    source_back_low = min(source_back_low, source_phrase_low);
    source_back_high = max(source_back_high, source_phrase_high);

    // Stop if the reflected source span matches the previous source span.
    if (source_back_low == source_phrase_low &&
        source_back_high == source_phrase_high) {
      return true;
    }

    if (!allow_low_x && source_back_low < source_phrase_low) {
      // Extension on the left side not allowed.
      return false;
    }
    if (!allow_high_x && source_back_high > source_phrase_high) {
      // Extension on the right side not allowed.
      return false;
    }

    // Extend left side.
    if (source_back_low < source_phrase_low) {
      if (new_low_x == false) {
        if (new_x >= max_new_x) {
          return false;
        }
        new_low_x = true;
        ++new_x;
      }
      if (source_phrase_low - source_back_low < min_source_gap_size) {
        source_back_low = source_phrase_low - min_source_gap_size;
        if (source_back_low < 0) {
          return false;
        }
      }
    }

    // Extend right side.
    if (source_back_high > source_phrase_high) {
      if (new_high_x == false) {
        if (new_x >= max_new_x) {
          return false;
        }
        new_high_x = true;
        ++new_x;
      }
      if (source_back_high - source_phrase_high < min_source_gap_size) {
        source_back_high = source_phrase_high + min_source_gap_size;
        if (source_back_high > source_sent_len) {
          return false;
        }
      }
    }

    if (source_back_high - source_back_low > max_rule_span) {
      // Rule span too wide.
      return false;
    }

    prev_target_low = target_phrase_low;
    prev_target_high = target_phrase_high;
    // Find the reflection including the left gap (if one was added).
    FindProjection(source_back_low, source_phrase_low, source_low, source_high,
                   target_phrase_low, target_phrase_high);
    // Find the reflection including the right gap (if one was added).
    FindProjection(source_phrase_high, source_back_high, source_low,
                   source_high, target_phrase_low, target_phrase_high);
    // Stop if the new re-reflected target span matches the previous target
    // span.
    if (prev_target_low == target_phrase_low &&
        prev_target_high == target_phrase_high) {
      return true;
    }

    if (!allow_arbitrary_expansion) {
      // Arbitrary expansion not allowed.
      return false;
    }
    if (target_phrase_high - target_phrase_low > max_rule_span) {
      // Target side too wide.
      return false;
    }

    source_phrase_low = source_back_low;
    source_phrase_high = source_back_high;
    // Re-reflect the target span.
    FindProjection(target_phrase_low, prev_target_low, target_low, target_high,
                   source_back_low, source_back_high);
    FindProjection(prev_target_high, target_phrase_high, target_low,
                   target_high, source_back_low, source_back_high);
  }

  return false;
}

void RuleExtractorHelper::FindProjection(
    int source_phrase_low, int source_phrase_high,
    const vector<int>& source_low, const vector<int>& source_high,
    int& target_phrase_low, int& target_phrase_high) const {
  for (size_t i = source_phrase_low; i < source_phrase_high; ++i) {
    if (source_low[i] != -1) {
      if (target_phrase_low == -1 || source_low[i] < target_phrase_low) {
        target_phrase_low = source_low[i];
      }
      target_phrase_high = max(target_phrase_high, source_high[i]);
    }
  }
}

bool RuleExtractorHelper::GetGaps(
     vector<pair<int, int> >& source_gaps, vector<pair<int, int> >& target_gaps,
     const vector<int>& matching, const vector<int>& chunklen,
     const vector<int>& source_low, const vector<int>& source_high,
     const vector<int>& target_low, const vector<int>& target_high,
     int source_phrase_low, int source_phrase_high, int source_back_low,
     int source_back_high, int sentence_id, int source_sent_start,
     int& num_symbols, bool& met_constraints) const {
  if (source_back_low < source_phrase_low) {
    source_gaps.push_back(make_pair(source_back_low, source_phrase_low));
    if (num_symbols >= max_rule_symbols) {
      // Source side contains too many symbols.
      return false;
    }
    ++num_symbols;
    if (require_tight_phrases && (source_low[source_back_low] == -1 ||
        source_low[source_phrase_low - 1] == -1)) {
      // Inside edges of preceding gap are not tight.
      return false;
    }
  } else if (require_tight_phrases && source_low[source_phrase_low] == -1) {
    // This is not a hard error. We can't extract this phrase, but we might
    // still be able to extract a superphrase.
    met_constraints = false;
  }

  for (size_t i = 0; i + 1 < chunklen.size(); ++i) {
    int gap_start = matching[i] + chunklen[i] - source_sent_start;
    int gap_end = matching[i + 1] - source_sent_start;
    source_gaps.push_back(make_pair(gap_start, gap_end));
  }

  if (source_phrase_high < source_back_high) {
    source_gaps.push_back(make_pair(source_phrase_high, source_back_high));
    if (num_symbols >= max_rule_symbols) {
      // Source side contains too many symbols.
      return false;
    }
    ++num_symbols;
    if (require_tight_phrases && (source_low[source_phrase_high] == -1 ||
        source_low[source_back_high - 1] == -1)) {
      // Inside edges of following gap are not tight.
      return false;
    }
  } else if (require_tight_phrases &&
             source_low[source_phrase_high - 1] == -1) {
    // This is not a hard error. We can't extract this phrase, but we might
    // still be able to extract a superphrase.
    met_constraints = false;
  }

  target_gaps.resize(source_gaps.size(), make_pair(-1, -1));
  for (size_t i = 0; i < source_gaps.size(); ++i) {
    if (!FindFixPoint(source_gaps[i].first, source_gaps[i].second, source_low,
                      source_high, target_gaps[i].first, target_gaps[i].second,
                      target_low, target_high, source_gaps[i].first,
                      source_gaps[i].second, sentence_id, 0, 0, 0, false, false,
                      false)) {
      // Gap fails integrity check.
      return false;
    }
  }

  return true;
}

vector<int> RuleExtractorHelper::GetGapOrder(
    const vector<pair<int, int> >& gaps) const {
  vector<int> gap_order(gaps.size());
  for (size_t i = 0; i < gap_order.size(); ++i) {
    for (size_t j = 0; j < i; ++j) {
      if (gaps[gap_order[j]] < gaps[i]) {
        ++gap_order[i];
      } else {
        ++gap_order[j];
      }
    }
  }
  return gap_order;
}

unordered_map<int, int> RuleExtractorHelper::GetSourceIndexes(
    const vector<int>& matching, const vector<int>& chunklen,
    int starts_with_x, int source_sent_start) const {
 unordered_map<int, int> source_indexes;
 int num_symbols = starts_with_x;
 for (size_t i = 0; i < matching.size(); ++i) {
   for (size_t j = 0; j < chunklen[i]; ++j) {
     source_indexes[matching[i] + j - source_sent_start] = num_symbols;
     ++num_symbols;
   }
   ++num_symbols;
 }
 return source_indexes;
}

} // namespace extractor
