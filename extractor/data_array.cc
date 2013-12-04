#include "data_array.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

using namespace std;

namespace extractor {

int DataArray::NULL_WORD = 0;
int DataArray::END_OF_LINE = 1;
string DataArray::NULL_WORD_STR = "__NULL__";
string DataArray::END_OF_LINE_STR = "__END_OF_LINE__";

DataArray::DataArray() {
  InitializeDataArray();
}

DataArray::DataArray(const string& filename) {
  InitializeDataArray();
  ifstream infile(filename.c_str());
  vector<string> lines;
  string line;
  while (getline(infile, line)) {
    lines.push_back(line);
  }
  CreateDataArray(lines);
}

DataArray::DataArray(const string& filename, const Side& side) {
  InitializeDataArray();
  ifstream infile(filename.c_str());
  vector<string> lines;
  string line, delimiter = "|||";
  while (getline(infile, line)) {
    int position = line.find(delimiter);
    if (side == SOURCE) {
      lines.push_back(line.substr(0, position));
    } else {
      lines.push_back(line.substr(position + delimiter.size()));
    }
  }
  CreateDataArray(lines);
}

void DataArray::InitializeDataArray() {
  word2id[NULL_WORD_STR] = NULL_WORD;
  id2word.push_back(NULL_WORD_STR);
  word2id[END_OF_LINE_STR] = END_OF_LINE;
  id2word.push_back(END_OF_LINE_STR);
}

void DataArray::CreateDataArray(const vector<string>& lines) {
  for (size_t i = 0; i < lines.size(); ++i) {
    sentence_start.push_back(data.size());

    istringstream iss(lines[i]);
    string word;
    while (iss >> word) {
      if (word2id.count(word) == 0) {
        word2id[word] = id2word.size();
        id2word.push_back(word);
      }
      data.push_back(word2id[word]);
      sentence_id.push_back(i);
    }
    data.push_back(END_OF_LINE);
    sentence_id.push_back(i);
  }
  sentence_start.push_back(data.size());

  data.shrink_to_fit();
  sentence_id.shrink_to_fit();
  sentence_start.shrink_to_fit();
}

DataArray::~DataArray() {}

vector<int> DataArray::GetData() const {
  return data;
}

int DataArray::AtIndex(int index) const {
  return data[index];
}

string DataArray::GetWordAtIndex(int index) const {
  return id2word[data[index]];
}

vector<int> DataArray::GetWordIds(int index, int size) const {
  return vector<int>(data.begin() + index, data.begin() + index + size);
}

vector<string> DataArray::GetWords(int start_index, int size) const {
  vector<string> words;
  for (int word_id: GetWordIds(start_index, size)) {
    words.push_back(id2word[word_id]);
  }
  return words;
}

int DataArray::GetSize() const {
  return data.size();
}

int DataArray::GetVocabularySize() const {
  return id2word.size();
}

int DataArray::GetNumSentences() const {
  return sentence_start.size() - 1;
}

int DataArray::GetSentenceStart(int position) const {
  return sentence_start[position];
}

int DataArray::GetSentenceLength(int sentence_id) const {
  // Ignore end of line markers.
  return sentence_start[sentence_id + 1] - sentence_start[sentence_id] - 1;
}

int DataArray::GetSentenceId(int position) const {
  return sentence_id[position];
}

int DataArray::GetWordId(const string& word) const {
  auto result = word2id.find(word);
  return result == word2id.end() ? -1 : result->second;
}

string DataArray::GetWord(int word_id) const {
  return id2word[word_id];
}

bool DataArray::operator==(const DataArray& other) const {
  return word2id == other.word2id && id2word == other.id2word &&
         data == other.data && sentence_start == other.sentence_start &&
         sentence_id == other.sentence_id;
}

} // namespace extractor
