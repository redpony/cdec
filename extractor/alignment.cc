#include "alignment.h"

#include <fstream>
#include <sstream>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <vector>

#include <boost/algorithm/string.hpp>

using namespace std;

namespace extractor {

Alignment::Alignment(const string& filename) {
  ifstream infile(filename.c_str());
  string line;
  while (getline(infile, line)) {
    vector<string> items;
    boost::split(items, line, boost::is_any_of(" -"));
    vector<pair<int, int>> alignment;
    alignment.reserve(items.size() / 2);
    for (size_t i = 1; i < items.size(); i += 2) {
      alignment.push_back(make_pair(stoi(items[i - 1]), stoi(items[i])));
    }
    alignments.push_back(alignment);
  }
  alignments.shrink_to_fit();
}

Alignment::Alignment() {}

Alignment::~Alignment() {}

vector<pair<int, int>> Alignment::GetLinks(int sentence_index) const {
  return alignments[sentence_index];
}

bool Alignment::operator==(const Alignment& other) const {
  return alignments == other.alignments;
}

} // namespace extractor
