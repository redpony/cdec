#ifndef _TRANSLATION_TABLE_
#define _TRANSLATION_TABLE_

#include <memory>
#include <string>
#include <unordered_map>

#include <boost/functional/hash.hpp>
#include <boost/serialization/serialization.hpp>
#include <boost/serialization/split_member.hpp>
#include <boost/serialization/utility.hpp>

using namespace std;

namespace extractor {

typedef boost::hash<pair<int, int>> PairHash;

class Alignment;
class DataArray;

/**
 * Bilexical table with conditional probabilities.
 */
class TranslationTable {
 public:
  // Constructs a translation table from source data, target data and the
  // corresponding alignment.
  TranslationTable(
      shared_ptr<DataArray> source_data_array,
      shared_ptr<DataArray> target_data_array,
      shared_ptr<Alignment> alignment);

  // Creates empty translation table.
  TranslationTable();

  virtual ~TranslationTable();

  // Returns p(e | f).
  virtual double GetTargetGivenSourceScore(const string& source_word,
                                           const string& target_word);

  // Returns p(f | e).
  virtual double GetSourceGivenTargetScore(const string& source_word,
                                           const string& target_word);

  bool operator==(const TranslationTable& other) const;

 private:
  // Increment links count for the given (f, e) word pair.
  void IncrementLinksCount(
      unordered_map<int, int>& source_links_count,
      unordered_map<int, int>& target_links_count,
      unordered_map<pair<int, int>, int, PairHash>& links_count,
      int source_word_id,
      int target_word_id) const;

  friend class boost::serialization::access;

  template<class Archive> void save(Archive& ar, unsigned int) const {
    ar << *source_data_array << *target_data_array;

    int num_entries = translation_probabilities.size();
    ar << num_entries;
    for (auto entry: translation_probabilities) {
      ar << entry;
    }
  }

  template<class Archive> void load(Archive& ar, unsigned int) {
    source_data_array = make_shared<DataArray>();
    ar >> *source_data_array;
    target_data_array = make_shared<DataArray>();
    ar >> *target_data_array;

    int num_entries;
    ar >> num_entries;
    for (size_t i = 0; i < num_entries; ++i) {
      pair<pair<int, int>, pair<double, double>> entry;
      ar >> entry;
      translation_probabilities.insert(entry);
    }
  }

  BOOST_SERIALIZATION_SPLIT_MEMBER();

  shared_ptr<DataArray> source_data_array;
  shared_ptr<DataArray> target_data_array;
  unordered_map<pair<int, int>, pair<double, double>, PairHash>
      translation_probabilities;
};

} // namespace extractor

#endif
