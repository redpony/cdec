#include "suffix_array.h"

#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include "data_array.h"
#include "phrase_location.h"
#include "time_util.h"

namespace fs = boost::filesystem;
using namespace std;
using namespace chrono;

namespace extractor {

SuffixArray::SuffixArray(shared_ptr<DataArray> data_array) :
    data_array(data_array) {
  BuildSuffixArray();
}

SuffixArray::SuffixArray() {}

SuffixArray::~SuffixArray() {}

void SuffixArray::BuildSuffixArray() {
  vector<int> groups = data_array->GetData();
  groups.reserve(groups.size() + 1);
  groups.push_back(DataArray::NULL_WORD);
  suffix_array.resize(groups.size());
  word_start.resize(data_array->GetVocabularySize() + 1);

  InitialBucketSort(groups);

  int combined_group_size = 0;
  for (size_t i = 1; i < word_start.size(); ++i) {
    if (word_start[i] - word_start[i - 1] == 1) {
      ++combined_group_size;
      suffix_array[word_start[i] - combined_group_size] = -combined_group_size;
    } else {
      combined_group_size = 0;
    }
  }

  PrefixDoublingSort(groups);
  cerr << "\tFinalizing sort..." << endl;

  for (size_t i = 0; i < groups.size(); ++i) {
    suffix_array[groups[i]] = i;
  }
}

void SuffixArray::InitialBucketSort(vector<int>& groups) {
  Clock::time_point start_time = Clock::now();
  for (size_t i = 0; i < groups.size(); ++i) {
    ++word_start[groups[i]];
  }

  for (size_t i = 1; i < word_start.size(); ++i) {
    word_start[i] += word_start[i - 1];
  }

  for (size_t i = 0; i < groups.size(); ++i) {
    --word_start[groups[i]];
    suffix_array[word_start[groups[i]]] = i;
  }

  for (size_t i = 0; i < suffix_array.size(); ++i) {
    groups[i] = word_start[groups[i] + 1] - 1;
  }
  Clock::time_point stop_time = Clock::now();
  cerr << "\tBucket sort took " << GetDuration(start_time, stop_time)
       << " seconds" << endl;
}

void SuffixArray::PrefixDoublingSort(vector<int>& groups) {
  int step = 1;
  while (suffix_array[0] != -suffix_array.size()) {
    int combined_group_size = 0;
    int i = 0;
    while (i < suffix_array.size()) {
      if (suffix_array[i] < 0) {
        int skip = -suffix_array[i];
        combined_group_size += skip;
        i += skip;
        suffix_array[i - combined_group_size] = -combined_group_size;
      } else {
        combined_group_size = 0;
        int j = groups[suffix_array[i]];
        TernaryQuicksort(i, j, step, groups);
        i = j + 1;
      }
    }
    step *= 2;
  }
}

void SuffixArray::TernaryQuicksort(int left, int right, int step,
    vector<int>& groups) {
  if (left > right) {
    return;
  }

  int pivot = left + rand() % (right - left + 1);
  int pivot_value = groups[suffix_array[pivot] + step];
  swap(suffix_array[pivot], suffix_array[left]);
  int mid_left = left, mid_right = left;
  for (int i = left + 1; i <= right; ++i) {
    if (groups[suffix_array[i] + step] < pivot_value) {
      ++mid_right;
      int temp = suffix_array[i];
      suffix_array[i] = suffix_array[mid_right];
      suffix_array[mid_right] = suffix_array[mid_left];
      suffix_array[mid_left] = temp;
      ++mid_left;
    } else if (groups[suffix_array[i] + step] == pivot_value) {
      ++mid_right;
      int temp = suffix_array[i];
      suffix_array[i] = suffix_array[mid_right];
      suffix_array[mid_right] = temp;
    }
  }

  TernaryQuicksort(left, mid_left - 1, step, groups);

  if (mid_left == mid_right) {
    groups[suffix_array[mid_left]] = mid_left;
    suffix_array[mid_left] = -1;
  } else {
    for (int i = mid_left; i <= mid_right; ++i) {
      groups[suffix_array[i]] = mid_right;
    }
  }

  TernaryQuicksort(mid_right + 1, right, step, groups);
}

vector<int> SuffixArray::BuildLCPArray() const {
  Clock::time_point start_time = Clock::now();
  cerr << "\tConstructing LCP array..." << endl;

  vector<int> lcp(suffix_array.size());
  vector<int> rank(suffix_array.size());
  const vector<int>& data = data_array->GetData();

  for (size_t i = 0; i < suffix_array.size(); ++i) {
    rank[suffix_array[i]] = i;
  }

  int prefix_len = 0;
  for (size_t i = 0; i < suffix_array.size(); ++i) {
    if (rank[i] == 0) {
      lcp[rank[i]] = -1;
    } else {
      int j = suffix_array[rank[i] - 1];
      while (i + prefix_len < data.size() && j + prefix_len < data.size()
          && data[i + prefix_len] == data[j + prefix_len]) {
        ++prefix_len;
      }
      lcp[rank[i]] = prefix_len;
    }

    if (prefix_len > 0) {
      --prefix_len;
    }
  }

  Clock::time_point stop_time = Clock::now();
  cerr << "\tConstructing LCP took "
       << GetDuration(start_time, stop_time) << " seconds" << endl;

  return lcp;
}

int SuffixArray::GetSuffix(int rank) const {
  return suffix_array[rank];
}

int SuffixArray::GetSize() const {
  return suffix_array.size();
}

shared_ptr<DataArray> SuffixArray::GetData() const {
  return data_array;
}

void SuffixArray::WriteBinary(const fs::path& filepath) const {
  FILE* file = fopen(filepath.string().c_str(), "w");
  assert(file);
  data_array->WriteBinary(file);

  int size = suffix_array.size();
  fwrite(&size, sizeof(int), 1, file);
  fwrite(suffix_array.data(), sizeof(int), size, file);

  size = word_start.size();
  fwrite(&size, sizeof(int), 1, file);
  fwrite(word_start.data(), sizeof(int), size, file);
}

PhraseLocation SuffixArray::Lookup(int low, int high, const string& word,
                                   int offset) const {
  if (!data_array->HasWord(word)) {
    // Return empty phrase location.
    return PhraseLocation(0, 0);
  }

  int word_id = data_array->GetWordId(word);
  if (offset == 0) {
    return PhraseLocation(word_start[word_id], word_start[word_id + 1]);
  }

  return PhraseLocation(LookupRangeStart(low, high, word_id, offset),
      LookupRangeStart(low, high, word_id + 1, offset));
}

int SuffixArray::LookupRangeStart(int low, int high, int word_id,
                                  int offset) const {
  int result = high;
  while (low < high) {
    int middle = low + (high - low) / 2;
    if (suffix_array[middle] + offset >= data_array->GetSize() ||
        data_array->AtIndex(suffix_array[middle] + offset) < word_id) {
      low = middle + 1;
    } else {
      result = middle;
      high = middle;
    }
  }
  return result;
}

} // namespace extractor
