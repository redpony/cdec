#include "lattice.h"

#include "tdict.h"
#include "hg_io.h"

using namespace std;

bool LatticeTools::LooksLikePLF(const string &line) {
  return (line.size() > 5) && (line.substr(0,4) == "((('");
}

void LatticeTools::ConvertTextToLattice(const string& text, Lattice* pl) {
  Lattice& l = *pl;
  vector<WordID> ids;
  TD::ConvertSentence(text, &ids);
  l.resize(ids.size());
  for (int i = 0; i < l.size(); ++i)
    l[i].push_back(LatticeArc(ids[i], 0.0, 1));
}

void LatticeTools::ConvertTextOrPLF(const string& text_or_plf, Lattice* pl) {
  if (LooksLikePLF(text_or_plf))
    HypergraphIO::PLFtoLattice(text_or_plf, pl);
  else
    ConvertTextToLattice(text_or_plf, pl);
}

