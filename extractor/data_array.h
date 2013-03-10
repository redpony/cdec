#ifndef _DATA_ARRAY_H_
#define _DATA_ARRAY_H_

#include <string>
#include <unordered_map>
#include <vector>

#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;
using namespace std;

namespace extractor {

enum Side {
  SOURCE,
  TARGET
};

/**
 * Data structure storing information about a single side of a parallel corpus.
 *
 * Each word is mapped to a unique integer (word_id). The data structure holds
 * the corpus in the numberized format, together with the hash table mapping
 * words to word_ids. It also holds additional information such as the starting
 * index for each sentence and, for each token, the index of the sentence it
 * belongs to.
 *
 * Note: This class has features for both the source and target data arrays.
 * Maybe we can save some memory by having more specific implementations (not
 * likely to save a lot of memory tough).
 */
class DataArray {
 public:
  static int NULL_WORD;
  static int END_OF_LINE;
  static string NULL_WORD_STR;
  static string END_OF_LINE_STR;

  // Reads data array from text file.
  DataArray(const string& filename);

  // Reads data array from bitext file where the sentences are separated by |||.
  DataArray(const string& filename, const Side& side);

  virtual ~DataArray();

  // Returns a vector containing the word ids.
  virtual const vector<int>& GetData() const;

  // Returns the word id at the specified position.
  virtual int AtIndex(int index) const;

  // Returns the original word at the specified position.
  virtual string GetWordAtIndex(int index) const;

  // Returns the size of the data array.
  virtual int GetSize() const;

  // Returns the number of distinct words in the data array.
  virtual int GetVocabularySize() const;

  // Returns whether a word has ever been observed in the data array.
  virtual bool HasWord(const string& word) const;

  // Returns the word id for a given word or -1 if it the word has never been
  // observed.
  virtual int GetWordId(const string& word) const;

  // Returns the word corresponding to a particular word id.
  virtual string GetWord(int word_id) const;

  // Returns the number of sentences in the data.
  virtual int GetNumSentences() const;

  // Returns the index where the sentence containing the given position starts.
  virtual int GetSentenceStart(int position) const;

  // Returns the length of the sentence.
  virtual int GetSentenceLength(int sentence_id) const;

  // Returns the number of the sentence containing the given position.
  virtual int GetSentenceId(int position) const;

  // Writes data array to file in binary format.
  void WriteBinary(const fs::path& filepath) const;

  // Writes data array to file in binary format.
  void WriteBinary(FILE* file) const;

 protected:
  DataArray();

 private:
  // Sets up specific constants.
  void InitializeDataArray();

  // Constructs the data array.
  void CreateDataArray(const vector<string>& lines);

  unordered_map<string, int> word2id;
  vector<string> id2word;
  vector<int> data;
  vector<int> sentence_id;
  vector<int> sentence_start;
};

} // namespace extractor

#endif
