#include "precomputation.h"

#include <iostream>
#include <queue>

#include "data_array.h"
#include "suffix_array.h"

using namespace std;

namespace extractor {

int Precomputation::FIRST_NONTERMINAL = -1;
int Precomputation::SECOND_NONTERMINAL = -2;

Precomputation::Precomputation(
    shared_ptr<SuffixArray> suffix_array, int num_frequent_phrases,
    int num_super_frequent_phrases, int max_rule_span,
    int max_rule_symbols, int min_gap_size,
    int max_frequent_phrase_len, int min_frequency) {
  vector<int> data = suffix_array->GetData()->GetData();
  vector<vector<int>> frequent_phrases = FindMostFrequentPhrases(
      suffix_array, data, num_frequent_phrases, max_frequent_phrase_len,
      min_frequency);

  // Construct sets containing the frequent and superfrequent contiguous
  // collocations.
  unordered_set<vector<int>, VectorHash> frequent_phrases_set;
  unordered_set<vector<int>, VectorHash> super_frequent_phrases_set;
  for (size_t i = 0; i < frequent_phrases.size(); ++i) {
    frequent_phrases_set.insert(frequent_phrases[i]);
    if (i < num_super_frequent_phrases) {
      super_frequent_phrases_set.insert(frequent_phrases[i]);
    }
  }

  vector<tuple<int, int, int>> locations;
  for (size_t i = 0; i < data.size(); ++i) {
    // If the sentence is over, add all the discontiguous frequent phrases to
    // the list.
    if (data[i] == DataArray::END_OF_LINE) {
      AddCollocations(locations, data, max_rule_span, min_gap_size,
          max_rule_symbols);
      locations.clear();
      continue;
    }
    vector<int> phrase;
    // Find all the contiguous frequent phrases starting at position i.
    for (int j = 1; j <= max_frequent_phrase_len && i + j <= data.size(); ++j) {
      phrase.push_back(data[i + j - 1]);
      if (frequent_phrases_set.count(phrase)) {
        int is_super_frequent = super_frequent_phrases_set.count(phrase);
        locations.push_back(make_tuple(i, j, is_super_frequent));
      } else {
        // If the current phrase is not frequent, any longer phrase having the
        // current phrase as prefix will not be frequent.
        break;
      }
    }
  }

  collocations.shrink_to_fit();
}

Precomputation::Precomputation() {}

Precomputation::~Precomputation() {}

vector<vector<int>> Precomputation::FindMostFrequentPhrases(
    shared_ptr<SuffixArray> suffix_array, const vector<int>& data,
    int num_frequent_phrases, int max_frequent_phrase_len, int min_frequency) {
  vector<int> lcp = suffix_array->BuildLCPArray();
  vector<int> run_start(max_frequent_phrase_len);

  // Find all the phrases occurring at least min_frequency times.
  priority_queue<pair<int, pair<int, int>>> heap;
  for (size_t i = 1; i < lcp.size(); ++i) {
    for (int len = lcp[i]; len < max_frequent_phrase_len; ++len) {
      int frequency = i - run_start[len];
      if (frequency >= min_frequency) {
        heap.push(make_pair(frequency,
            make_pair(suffix_array->GetSuffix(run_start[len]), len + 1)));
      }
      run_start[len] = i;
    }
  }

  // Extract the most frequent phrases.
  vector<vector<int>> frequent_phrases;
  while (frequent_phrases.size() < num_frequent_phrases && !heap.empty()) {
    int start = heap.top().second.first;
    int len = heap.top().second.second;
    heap.pop();

    vector<int> phrase(data.begin() + start, data.begin() + start + len);
    if (find(phrase.begin(), phrase.end(), DataArray::END_OF_LINE) ==
        phrase.end()) {
      frequent_phrases.push_back(phrase);
    }
  }
  return frequent_phrases;
}

void Precomputation::AddCollocations(
    const vector<tuple<int, int, int>>& locations, const vector<int>& data,
    int max_rule_span, int min_gap_size, int max_rule_symbols) {
  // Select the leftmost subphrase.
  for (size_t i = 0; i < locations.size(); ++i) {
    int start1, size1, is_super1;
    tie(start1, size1, is_super1) = locations[i];

    // Select the second (middle) subphrase
    for (size_t j = i + 1; j < locations.size(); ++j) {
      int start2, size2, is_super2;
      tie(start2, size2, is_super2) = locations[j];
      if (start2 - start1 >= max_rule_span) {
        break;
      }

      if (start2 - start1 - size1 >= min_gap_size
          && start2 + size2 - start1 <= max_rule_span
          && size1 + size2 + 1 <= max_rule_symbols) {
        vector<int> collocation(data.begin() + start1,
            data.begin() + start1 + size1);
        collocation.push_back(Precomputation::FIRST_NONTERMINAL);
        collocation.insert(collocation.end(), data.begin() + start2,
            data.begin() + start2 + size2);

        AddCollocation(collocation, GetLocation(start1, start2));

        // Try extending the binary collocation to a ternary collocation.
        if (is_super2) {
          collocation.push_back(Precomputation::SECOND_NONTERMINAL);
          // Select the rightmost subphrase.
          for (size_t k = j + 1; k < locations.size(); ++k) {
            int start3, size3, is_super3;
            tie(start3, size3, is_super3) = locations[k];
            if (start3 - start1 >= max_rule_span) {
              break;
            }

            if (start3 - start2 - size2 >= min_gap_size
                && start3 + size3 - start1 <= max_rule_span
                && size1 + size2 + size3 + 2 <= max_rule_symbols
                && (is_super1 || is_super3)) {
              collocation.insert(collocation.end(), data.begin() + start3,
                  data.begin() + start3 + size3);

              AddCollocation(collocation, GetLocation(start1, start2, start3));

              collocation.erase(collocation.end() - size3);
            }
          }
        }
      }
    }
  }
}

vector<int> Precomputation::GetLocation(int pos1, int pos2) {
  vector<int> location;
  location.push_back(pos1);
  location.push_back(pos2);
  return location;
}

vector<int> Precomputation::GetLocation(int pos1, int pos2, int pos3) {
  vector<int> location;
  location.push_back(pos1);
  location.push_back(pos2);
  location.push_back(pos3);
  return location;
}

void Precomputation::AddCollocation(vector<int> collocation,
                                    vector<int> location) {
  collocation.shrink_to_fit();
  location.shrink_to_fit();
  collocations.push_back(make_pair(collocation, location));
}

Collocations Precomputation::GetCollocations() const {
  return collocations;
}

bool Precomputation::operator==(const Precomputation& other) const {
  return collocations == other.collocations;
}

} // namespace extractor
