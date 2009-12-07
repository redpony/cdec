#include <iostream>
#include <fstream>
#include <cassert>
#include "freqdict.h"
#include "tdict.h"
#include "filelib.h"

using namespace std;

void FreqDict::Load(const std::string& fname) {
  cerr << "Reading word frequencies: " << fname << endl;
  ReadFile rf(fname);
  istream& ifs = *rf.stream();
  int cc=0;
  while (ifs) {
    std::string word;
    ifs >> word;
    if (word.size() == 0) continue;
    if (word[0] == '#') continue;
    double count = 0;
    ifs >> count;
    assert(count > 0.0);  // use -log(f)
    counts_[TD::Convert(word)]=count;
    ++cc;
    if (cc % 10000 == 0) { std::cerr << "."; }
  }
  std::cerr << "\n";
  std::cerr << "Loaded " << cc << " words\n";
}
