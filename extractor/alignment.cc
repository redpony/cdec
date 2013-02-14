#include "alignment.h"

#include <fstream>
#include <sstream>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;
using namespace std;

Alignment::Alignment(const string& filename) {
  ifstream infile(filename.c_str());
  string line;
  while (getline(infile, line)) {
    vector<string> items;
    boost::split(items, line, boost::is_any_of(" -"));
    vector<pair<int, int> > alignment;
    alignment.reserve(items.size() / 2);
    for (size_t i = 0; i < items.size(); i += 2) {
      alignment.push_back(make_pair(stoi(items[i]), stoi(items[i + 1])));
    }
    alignments.push_back(alignment);
  }
  // Note: shrink_to_fit does nothing for vector<vector<string> > on g++ 4.6.3,
  // but let's hope that the bug will be fixed in a newer version.
  alignments.shrink_to_fit();
}

Alignment::Alignment() {}

Alignment::~Alignment() {}

vector<pair<int, int> > Alignment::GetLinks(int sentence_index) const {
  return alignments[sentence_index];
}

void Alignment::WriteBinary(const fs::path& filepath) {
  FILE* file = fopen(filepath.string().c_str(), "w");
  int size = alignments.size();
  fwrite(&size, sizeof(int), 1, file);
  for (vector<pair<int, int> > alignment: alignments) {
    size = alignment.size();
    fwrite(&size, sizeof(int), 1, file);
    fwrite(alignment.data(), sizeof(pair<int, int>), size, file);
  }
}
