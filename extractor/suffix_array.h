#ifndef _SUFFIX_ARRAY_H_
#define _SUFFIX_ARRAY_H_

#include <memory>
#include <string>
#include <vector>

#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;
using namespace std;

namespace extractor {

class DataArray;
class PhraseLocation;

class SuffixArray {
 public:
  SuffixArray(shared_ptr<DataArray> data_array);

  virtual ~SuffixArray();

  virtual int GetSize() const;

  virtual shared_ptr<DataArray> GetData() const;

  virtual vector<int> BuildLCPArray() const;

  virtual int GetSuffix(int rank) const;

  virtual PhraseLocation Lookup(int low, int high, const string& word,
                                int offset) const;

  void WriteBinary(const fs::path& filepath) const;

 protected:
  SuffixArray();

 private:
  void BuildSuffixArray();

  void InitialBucketSort(vector<int>& groups);

  void TernaryQuicksort(int left, int right, int step, vector<int>& groups);

  void PrefixDoublingSort(vector<int>& groups);

  int LookupRangeStart(int low, int high, int word_id, int offset) const;

  shared_ptr<DataArray> data_array;
  vector<int> suffix_array;
  vector<int> word_start;
};

} // namespace extractor

#endif
