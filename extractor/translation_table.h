#ifndef _TRANSLATION_TABLE_
#define _TRANSLATION_TABLE_

#include <memory>
#include <string>
#include <tr1/unordered_map>

#include <boost/filesystem.hpp>
#include <boost/functional/hash.hpp>

using namespace std;
using namespace tr1;
namespace fs = boost::filesystem;

class Alignment;
class DataArray;

class TranslationTable {
 public:
  TranslationTable(
      shared_ptr<DataArray> source_data_array,
      shared_ptr<DataArray> target_data_array,
      const Alignment& alignment);

  double GetEgivenFScore(const string& source_word, const string& target_word);

  double GetFgivenEScore(const string& source_word, const string& target_word);

  void WriteBinary(const fs::path& filepath) const;

 private:
   shared_ptr<DataArray> source_data_array;
   shared_ptr<DataArray> target_data_array;
   unordered_map<pair<int, int>, pair<double, double>,
      boost::hash<pair<int, int> > > translation_probabilities;
};

#endif
