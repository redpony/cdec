#include <iostream>
#include <fstream>
#include <cassert>
#include "freqdict.h"

void FreqDict::load(const std::string& fname) {
  std::ifstream ifs(fname.c_str());
  int cc=0;
  while (!ifs.eof()) {
    std::string word;
    ifs >> word;
    if (word.size() == 0) continue;
    if (word[0] == '#') continue;
    double count = 0;
    ifs >> count;
    assert(count > 0.0);  // use -log(f)
    counts_[word]=count;
    ++cc;
    if (cc % 10000 == 0) { std::cerr << "."; }
  }
  std::cerr << "\n";
  std::cerr << "Loaded " << cc << " words\n";
}
