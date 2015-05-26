#include "ttables.h"

#include <cassert>

#include "dict.h"

using namespace std;

void TTable::SerializeHelper(std::ostream& out, const TTable::Word2Word2Double params, const TTable::Word2Word2Double& viterbi, const double beam_threshold, const bool logsave) const {
  for (unsigned eind = 1; eind < params.size(); ++eind) {
    const auto& cd = params[eind];
    const TTable::Word2Double& vit = viterbi.at(eind);
    const string& esym = TD::Convert(eind);
    double max_c = -1;
    for (auto& fi : cd)
      if (fi.second > max_c) max_c = fi.second;
    const double threshold = max_c * beam_threshold;
    for (auto& fi : cd) {
      if (fi.second > threshold || (vit.find(fi.first) != vit.end())) {
        out << esym << ' ' << TD::Convert(fi.first) << ' ' << (logsave ? log(fi.second) : fi.second) << endl;
      }
    }
  }
}

void TTable::DeserializeHelper(std::istream* in, const bool logsaved, Word2Word2Double& target) {
  int c = 0;
  string e;
  string f;
  double p;
  while(*in) {
    (*in) >> e >> f >> p;
    if (e.empty()) break;
    ++c;
    WordID ie = TD::Convert(e);
    if (ie >= static_cast<int>(target.size())) target.resize(ie + 1);
    target[ie][TD::Convert(f)] = (logsaved) ? exp(p) : p;
  }
  cerr << "Loaded " << c << " parameters.\n";
}

