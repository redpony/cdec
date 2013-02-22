#ifndef _DATA_ARRAY_H_
#define _DATA_ARRAY_H_

#include <string>
#include <unordered_map>
#include <vector>

#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;
using namespace std;

enum Side {
  SOURCE,
  TARGET
};

// TODO: This class has features for both the source and target data arrays.
// Maybe we can save some memory by having more specific implementations (e.g.
// sentence_id is only needed for the source data array).
class DataArray {
 public:
  static int NULL_WORD;
  static int END_OF_LINE;
  static string NULL_WORD_STR;
  static string END_OF_LINE_STR;

  DataArray(const string& filename);

  DataArray(const string& filename, const Side& side);

  virtual ~DataArray();

  virtual const vector<int>& GetData() const;

  virtual int AtIndex(int index) const;

  virtual string GetWordAtIndex(int index) const;

  virtual int GetSize() const;

  virtual int GetVocabularySize() const;

  virtual bool HasWord(const string& word) const;

  virtual int GetWordId(const string& word) const;

  virtual string GetWord(int word_id) const;

  virtual int GetNumSentences() const;

  virtual int GetSentenceStart(int position) const;

  virtual int GetSentenceLength(int sentence_id) const;

  virtual int GetSentenceId(int position) const;

  void WriteBinary(const fs::path& filepath) const;

  void WriteBinary(FILE* file) const;

 protected:
  DataArray();

 private:
  void InitializeDataArray();
  void CreateDataArray(const vector<string>& lines);

  unordered_map<string, int> word2id;
  vector<string> id2word;
  vector<int> data;
  vector<int> sentence_id;
  vector<int> sentence_start;
};

#endif
