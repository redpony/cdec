#include "translation_table.h"

#include <string>
#include <vector>

#include <boost/functional/hash.hpp>

#include "alignment.h"
#include "data_array.h"

using namespace std;
using namespace tr1;

TranslationTable::TranslationTable(shared_ptr<DataArray> source_data_array,
                                   shared_ptr<DataArray> target_data_array,
                                   shared_ptr<Alignment> alignment) :
    source_data_array(source_data_array), target_data_array(target_data_array) {
  const vector<int>& source_data = source_data_array->GetData();
  const vector<int>& target_data = target_data_array->GetData();

  unordered_map<int, int> source_links_count;
  unordered_map<int, int> target_links_count;
  unordered_map<pair<int, int>, int, PairHash > links_count;

  for (size_t i = 0; i < source_data_array->GetNumSentences(); ++i) {
    const vector<pair<int, int> >& links = alignment->GetLinks(i);
    int source_start = source_data_array->GetSentenceStart(i);
    int next_source_start = source_data_array->GetSentenceStart(i + 1);
    int target_start = target_data_array->GetSentenceStart(i);
    int next_target_start = target_data_array->GetSentenceStart(i + 1);
    vector<int> source_sentence(source_data.begin() + source_start,
        source_data.begin() + next_source_start);
    vector<int> target_sentence(target_data.begin() + target_start,
        target_data.begin() + next_target_start);
    vector<int> source_linked_words(source_sentence.size());
    vector<int> target_linked_words(target_sentence.size());

    for (pair<int, int> link: links) {
      source_linked_words[link.first] = 1;
      target_linked_words[link.second] = 1;
      int source_word = source_sentence[link.first];
      int target_word = target_sentence[link.second];

      ++source_links_count[source_word];
      ++target_links_count[target_word];
      ++links_count[make_pair(source_word, target_word)];
    }

    // TODO(pauldb): Something seems wrong here. No NULL word?
  }

  for (pair<pair<int, int>, int> link_count: links_count) {
    int source_word = link_count.first.first;
    int target_word = link_count.first.second;
    double score1 = 1.0 * link_count.second / source_links_count[source_word];
    double score2 = 1.0 * link_count.second / target_links_count[target_word];
    translation_probabilities[link_count.first] = make_pair(score1, score2);
  }
}

double TranslationTable::GetTargetGivenSourceScore(
    const string& source_word, const string& target_word) {
  if (!source_data_array->HasWord(source_word) ||
      !target_data_array->HasWord(target_word)) {
    return -1;
  }

  int source_id = source_data_array->GetWordId(source_word);
  int target_id = target_data_array->GetWordId(target_word);
  return translation_probabilities[make_pair(source_id, target_id)].first;
}

double TranslationTable::GetSourceGivenTargetScore(
    const string& source_word, const string& target_word) {
  if (!source_data_array->HasWord(source_word) ||
      !target_data_array->HasWord(target_word) == 0) {
    return -1;
  }

  int source_id = source_data_array->GetWordId(source_word);
  int target_id = target_data_array->GetWordId(target_word);
  return translation_probabilities[make_pair(source_id, target_id)].second;
}

void TranslationTable::WriteBinary(const fs::path& filepath) const {
  FILE* file = fopen(filepath.string().c_str(), "w");

  int size = translation_probabilities.size();
  fwrite(&size, sizeof(int), 1, file);
  for (auto entry: translation_probabilities) {
    fwrite(&entry.first, sizeof(entry.first), 1, file);
    fwrite(&entry.second, sizeof(entry.second), 1, file);
  }
}
