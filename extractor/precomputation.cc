#include "precomputation.h"

#include <iostream>
#include <queue>

#include "data_array.h"
#include "suffix_array.h"
#include "time_util.h"
#include "vocabulary.h"

using namespace std;

namespace extractor {

Precomputation::Precomputation(
    shared_ptr<Vocabulary> vocabulary, shared_ptr<SuffixArray> suffix_array,
    int num_frequent_patterns, int num_super_frequent_patterns,
    int max_rule_span, int max_rule_symbols, int min_gap_size,
    int max_frequent_phrase_len, int min_frequency) {
  Clock::time_point start_time = Clock::now();
  shared_ptr<DataArray> data_array = suffix_array->GetData();
  vector<int> data = data_array->GetData();
  vector<vector<int>> frequent_patterns = FindMostFrequentPatterns(
      suffix_array, data, num_frequent_patterns, max_frequent_phrase_len,
      min_frequency);
  Clock::time_point end_time = Clock::now();
  cerr << "Finding most frequent patterns took "
       << GetDuration(start_time, end_time) << " seconds..." << endl;

  vector<vector<int>> pattern_annotations(frequent_patterns.size());
  unordered_map<vector<int>, int, VectorHash> frequent_patterns_index;
  for (size_t i = 0; i < frequent_patterns.size(); ++i) {
    frequent_patterns_index[frequent_patterns[i]] = i;
    pattern_annotations[i] = AnnotatePattern(vocabulary, data_array,
                                             frequent_patterns[i]);
  }

  start_time = Clock::now();
  vector<tuple<int, int, int>> matchings;
  vector<vector<int>> annotations;
  for (size_t i = 0; i < data.size(); ++i) {
    // If the sentence is over, add all the discontiguous frequent patterns to
    // the index.
    if (data[i] == DataArray::END_OF_LINE) {
      UpdateIndex(matchings, annotations, max_rule_span, min_gap_size,
                  max_rule_symbols);
      matchings.clear();
      annotations.clear();
      continue;
    }
    // Find all the contiguous frequent patterns starting at position i.
    vector<int> pattern;
    for (int j = 1; j <= max_frequent_phrase_len && i + j <= data.size(); ++j) {
      pattern.push_back(data[i + j - 1]);
      auto it = frequent_patterns_index.find(pattern);
      if (it == frequent_patterns_index.end()) {
        // If the current pattern is not frequent, any longer pattern having the
        // current pattern as prefix will not be frequent.
        break;
      }
      int is_super_frequent = it->second < num_super_frequent_patterns;
      matchings.push_back(make_tuple(i, j, is_super_frequent));
      annotations.push_back(pattern_annotations[it->second]);
    }
  }
  end_time = Clock::now();
  cerr << "Constructing collocations index took "
       << GetDuration(start_time, end_time) << " seconds..." << endl;
}

Precomputation::Precomputation() {}

Precomputation::~Precomputation() {}

vector<vector<int>> Precomputation::FindMostFrequentPatterns(
    shared_ptr<SuffixArray> suffix_array, const vector<int>& data,
    int num_frequent_patterns, int max_frequent_phrase_len, int min_frequency) {
  vector<int> lcp = suffix_array->BuildLCPArray();
  vector<int> run_start(max_frequent_phrase_len);

  // Find all the patterns occurring at least min_frequency times.
  priority_queue<pair<int, pair<int, int>>> heap;
  for (size_t i = 1; i < lcp.size(); ++i) {
    for (int len = lcp[i]; len < max_frequent_phrase_len; ++len) {
      int frequency = i - run_start[len];
      int start = suffix_array->GetSuffix(run_start[len]);
      if (frequency >= min_frequency && start + len <= data.size()) {
        heap.push(make_pair(frequency, make_pair(start, len + 1)));
      }
      run_start[len] = i;
    }
  }

  // Extract the most frequent patterns.
  vector<vector<int>> frequent_patterns;
  while (frequent_patterns.size() < num_frequent_patterns && !heap.empty()) {
    int start = heap.top().second.first;
    int len = heap.top().second.second;
    heap.pop();

    vector<int> pattern(data.begin() + start, data.begin() + start + len);
    if (find(pattern.begin(), pattern.end(), DataArray::END_OF_LINE) ==
        pattern.end()) {
      frequent_patterns.push_back(pattern);
    }
  }
  return frequent_patterns;
}

vector<int> Precomputation::AnnotatePattern(
    shared_ptr<Vocabulary> vocabulary, shared_ptr<DataArray> data_array,
    const vector<int>& pattern) const {
  vector<int> annotation;
  for (int word_id: pattern) {
    annotation.push_back(vocabulary->GetTerminalIndex(
        data_array->GetWord(word_id)));
  }
  return annotation;
}

void Precomputation::UpdateIndex(
    const vector<tuple<int, int, int>>& matchings,
    const vector<vector<int>>& annotations,
    int max_rule_span, int min_gap_size, int max_rule_symbols) {
  // Select the leftmost subpattern.
  for (size_t i = 0; i < matchings.size(); ++i) {
    int start1, size1, is_super1;
    tie(start1, size1, is_super1) = matchings[i];

    // Select the second (middle) subpattern
    for (size_t j = i + 1; j < matchings.size(); ++j) {
      int start2, size2, is_super2;
      tie(start2, size2, is_super2) = matchings[j];
      if (start2 - start1 >= max_rule_span) {
        break;
      }

      if (start2 - start1 - size1 >= min_gap_size
          && start2 + size2 - start1 <= max_rule_span
          && size1 + size2 + 1 <= max_rule_symbols) {
        vector<int> pattern = annotations[i];
        pattern.push_back(-1);
        AppendSubpattern(pattern, annotations[j]);
        AppendCollocation(index[pattern], start1, start2);

        // Try extending the binary collocation to a ternary collocation.
        if (is_super2) {
          pattern.push_back(-2);
          // Select the rightmost subpattern.
          for (size_t k = j + 1; k < matchings.size(); ++k) {
            int start3, size3, is_super3;
            tie(start3, size3, is_super3) = matchings[k];
            if (start3 - start1 >= max_rule_span) {
              break;
            }

            if (start3 - start2 - size2 >= min_gap_size
                && start3 + size3 - start1 <= max_rule_span
                && size1 + size2 + size3 + 2 <= max_rule_symbols
                && (is_super1 || is_super3)) {
              AppendSubpattern(pattern, annotations[k]);
              AppendCollocation(index[pattern], start1, start2, start3);
              pattern.erase(pattern.end() - size3);
            }
          }
        }
      }
    }
  }
}

void Precomputation::AppendSubpattern(
    vector<int>& pattern,
    const vector<int>& subpattern) {
  copy(subpattern.begin(), subpattern.end(), back_inserter(pattern));
}

void Precomputation::AppendCollocation(
    vector<int>& collocations, int pos1, int pos2) {
  collocations.push_back(pos1);
  collocations.push_back(pos2);
}

void Precomputation::AppendCollocation(
    vector<int>& collocations, int pos1, int pos2, int pos3) {
  collocations.push_back(pos1);
  collocations.push_back(pos2);
  collocations.push_back(pos3);
}

bool Precomputation::Contains(const vector<int>& pattern) const {
  return index.count(pattern);
}

vector<int> Precomputation::GetCollocations(const vector<int>& pattern) const {
  return index.at(pattern);
}

bool Precomputation::operator==(const Precomputation& other) const {
  return index == other.index;
}

} // namespace extractor
