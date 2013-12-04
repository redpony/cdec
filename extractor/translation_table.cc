#include "translation_table.h"

#include <string>
#include <vector>

#include <boost/functional/hash.hpp>

#include "alignment.h"
#include "data_array.h"

using namespace std;

namespace extractor {

TranslationTable::TranslationTable(shared_ptr<DataArray> source_data_array,
                                   shared_ptr<DataArray> target_data_array,
                                   shared_ptr<Alignment> alignment) :
    source_data_array(source_data_array), target_data_array(target_data_array) {
  const vector<int>& source_data = source_data_array->GetData();
  const vector<int>& target_data = target_data_array->GetData();

  unordered_map<int, int> source_links_count;
  unordered_map<int, int> target_links_count;
  unordered_map<pair<int, int>, int, PairHash> links_count;

  // For each pair of aligned source target words increment their link count by
  // 1. Unaligned words are paired with the NULL token.
  for (size_t i = 0; i < source_data_array->GetNumSentences(); ++i) {
    vector<pair<int, int>> links = alignment->GetLinks(i);
    int source_start = source_data_array->GetSentenceStart(i);
    int target_start = target_data_array->GetSentenceStart(i);
    // Ignore END_OF_LINE markers.
    int next_source_start = source_data_array->GetSentenceStart(i + 1) - 1;
    int next_target_start = target_data_array->GetSentenceStart(i + 1) - 1;
    vector<int> source_sentence(source_data.begin() + source_start,
        source_data.begin() + next_source_start);
    vector<int> target_sentence(target_data.begin() + target_start,
        target_data.begin() + next_target_start);
    vector<int> source_linked_words(source_sentence.size());
    vector<int> target_linked_words(target_sentence.size());

    for (pair<int, int> link: links) {
      source_linked_words[link.first] = 1;
      target_linked_words[link.second] = 1;
      IncrementLinksCount(source_links_count, target_links_count, links_count,
          source_sentence[link.first], target_sentence[link.second]);
    }

    for (size_t i = 0; i < source_sentence.size(); ++i) {
      if (!source_linked_words[i]) {
        IncrementLinksCount(source_links_count, target_links_count, links_count,
                            source_sentence[i], DataArray::NULL_WORD);
      }
    }

    for (size_t i = 0; i < target_sentence.size(); ++i) {
      if (!target_linked_words[i]) {
        IncrementLinksCount(source_links_count, target_links_count, links_count,
                            DataArray::NULL_WORD, target_sentence[i]);
      }
    }
  }

  // Calculating:
  //   p(e | f) = count(e, f) / count(f)
  //   p(f | e) = count(e, f) / count(e)
  for (pair<pair<int, int>, int> link_count: links_count) {
    int source_word = link_count.first.first;
    int target_word = link_count.first.second;
    double score1 = 1.0 * link_count.second / source_links_count[source_word];
    double score2 = 1.0 * link_count.second / target_links_count[target_word];
    translation_probabilities[link_count.first] = make_pair(score1, score2);
  }
}

TranslationTable::TranslationTable() {}

TranslationTable::~TranslationTable() {}

void TranslationTable::IncrementLinksCount(
    unordered_map<int, int>& source_links_count,
    unordered_map<int, int>& target_links_count,
    unordered_map<pair<int, int>, int, PairHash>& links_count,
    int source_word_id,
    int target_word_id) const {
  ++source_links_count[source_word_id];
  ++target_links_count[target_word_id];
  ++links_count[make_pair(source_word_id, target_word_id)];
}

double TranslationTable::GetTargetGivenSourceScore(
    const string& source_word, const string& target_word) {
  int source_id = source_data_array->GetWordId(source_word);
  int target_id = target_data_array->GetWordId(target_word);
  if (source_id == -1 || target_id == -1) {
    return -1;
  }

  auto entry = make_pair(source_id, target_id);
  auto it = translation_probabilities.find(entry);
  if (it == translation_probabilities.end()) {
    return 0;
  }
  return it->second.first;
}

double TranslationTable::GetSourceGivenTargetScore(
    const string& source_word, const string& target_word) {
  int source_id = source_data_array->GetWordId(source_word);
  int target_id = target_data_array->GetWordId(target_word);
  if (source_id == -1 || target_id == -1) {
    return -1;
  }

  auto entry = make_pair(source_id, target_id);
  auto it = translation_probabilities.find(entry);
  if (it == translation_probabilities.end()) {
    return 0;
  }
  return it->second.second;
}

bool TranslationTable::operator==(const TranslationTable& other) const {
  return *source_data_array == *other.source_data_array &&
         *target_data_array == *other.target_data_array &&
         translation_probabilities == other.translation_probabilities;
}

} // namespace extractor
