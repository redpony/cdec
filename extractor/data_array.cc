#include "data_array.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;
using namespace std;

int DataArray::END_OF_FILE = 0;
int DataArray::END_OF_LINE = 1;
string DataArray::END_OF_FILE_STR = "__END_OF_FILE__";
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
  word2id[END_OF_FILE_STR] = END_OF_FILE;
  id2word.push_back(END_OF_FILE_STR);
  word2id[END_OF_LINE_STR] = END_OF_FILE;
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

const vector<int>& DataArray::GetData() const {
  return data;
}

int DataArray::AtIndex(int index) const {
  return data[index];
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

int DataArray::GetSentenceId(int position) const {
  return sentence_id[position];
}

void DataArray::WriteBinary(const fs::path& filepath) const {
  WriteBinary(fopen(filepath.string().c_str(), "w"));
}

void DataArray::WriteBinary(FILE* file) const {
  int size = id2word.size();
  fwrite(&size, sizeof(int), 1, file);
  for (string word: id2word) {
    size = word.size();
    fwrite(&size, sizeof(int), 1, file);
    fwrite(word.data(), sizeof(char), size, file);
  }

  size = data.size();
  fwrite(&size, sizeof(int), 1, file);
  fwrite(data.data(), sizeof(int), size, file);

  size = sentence_id.size();
  fwrite(&size, sizeof(int), 1, file);
  fwrite(sentence_id.data(), sizeof(int), size, file);

  size = sentence_start.size();
  fwrite(&size, sizeof(int), 1, file);
  fwrite(sentence_start.data(), sizeof(int), 1, file);
}

bool DataArray::HasWord(const string& word) const {
  return word2id.count(word);
}

int DataArray::GetWordId(const string& word) const {
  return word2id.find(word)->second;
}

string DataArray::GetWord(int word_id) const {
  return id2word[word_id];
}
