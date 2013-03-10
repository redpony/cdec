#ifndef _TRANSLATION_TABLE_
#define _TRANSLATION_TABLE_

#include <memory>
#include <string>
#include <unordered_map>

#include <boost/filesystem.hpp>
#include <boost/functional/hash.hpp>

using namespace std;
namespace fs = boost::filesystem;

namespace extractor {

typedef boost::hash<pair<int, int> > PairHash;

class Alignment;
class DataArray;

/**
 * Bilexical table with conditional probabilities.
 */
class TranslationTable {
 public:
  TranslationTable(
      shared_ptr<DataArray> source_data_array,
      shared_ptr<DataArray> target_data_array,
      shared_ptr<Alignment> alignment);

  virtual ~TranslationTable();

  // Returns p(e | f).
  virtual double GetTargetGivenSourceScore(const string& source_word,
                                           const string& target_word);

  // Returns p(f | e).
  virtual double GetSourceGivenTargetScore(const string& source_word,
                                           const string& target_word);

  void WriteBinary(const fs::path& filepath) const;

 protected:
  TranslationTable();

 private:
  // Increment links count for the given (f, e) word pair.
  void IncrementLinksCount(
      unordered_map<int, int>& source_links_count,
      unordered_map<int, int>& target_links_count,
      unordered_map<pair<int, int>, int, PairHash>& links_count,
      int source_word_id,
      int target_word_id) const;

  shared_ptr<DataArray> source_data_array;
  shared_ptr<DataArray> target_data_array;
  unordered_map<pair<int, int>, pair<double, double>, PairHash>
      translation_probabilities;
};

} // namespace extractor

#endif
