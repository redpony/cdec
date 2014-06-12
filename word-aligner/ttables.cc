#include "ttables.h"

#include <cassert>

#include "dict.h"

using namespace std;

void TTable::DeserializeProbsFromText(std::istream* in) {
  int c = 0;
  string e;
  string f;
  double p;
  while(*in) {
    (*in) >> e >> f >> p;
    if (e.empty()) break;
    ++c;
    WordID ie = TD::Convert(e);
    if (ie >= static_cast<int>(ttable.size())) ttable.resize(ie + 1);
    ttable[ie][TD::Convert(f)] = p;
  }
  cerr << "Loaded " << c << " translation parameters.\n";
}

void TTable::DeserializeLogProbsFromText(std::istream* in) {
  int c = 0;
  string e;
  string f;
  double p;
  while(*in) {
    (*in) >> e >> f >> p;
    if (e.empty()) break;
    ++c;
    WordID ie = TD::Convert(e);
    if (ie >= static_cast<int>(ttable.size())) ttable.resize(ie + 1);
    ttable[ie][TD::Convert(f)] = exp(p);
  }
  cerr << "Loaded " << c << " translation parameters.\n";
}

void TTable::SerializeHelper(string* out, const Word2Word2Double& o) {
  assert(!"not implemented");
}

void TTable::DeserializeHelper(const string& in, Word2Word2Double* o) {
  assert(!"not implemented");
}

