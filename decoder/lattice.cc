#include "lattice.h"

#include "tdict.h"
#include "hg_io.h"

using namespace std;

static const int kUNREACHABLE = 99999999;

void Lattice::ComputeDistances() {
  const int n = this->size() + 1;
  dist_.resize(n, n, kUNREACHABLE);
  for (int i = 0; i < this->size(); ++i) {
    const vector<LatticeArc>& alts = (*this)[i];
    for (int j = 0; j < alts.size(); ++j)
      dist_(i, i + alts[j].dist2next) = 1;
  }
  for (int k = 0; k < n; ++k) {
    for (int i = 0; i < n; ++i) {
      for (int j = 0; j < n; ++j) {
        const int dp = dist_(i,k) + dist_(k,j);
        if (dist_(i,j) > dp)
          dist_(i,j) = dp;
      }
    }
  }

  for (int i = 0; i < n; ++i) {
    int latest = kUNREACHABLE;
    for (int j = n-1; j >= 0; --j) {
      const int c = dist_(i,j);
      if (c < kUNREACHABLE)
        latest = c;
      else
        dist_(i,j) = latest;
    }
  }
  // cerr << dist_ << endl;
}

bool LatticeTools::LooksLikePLF(const string &line) {
  return (line.size() > 5) && (line.substr(0,4) == "((('");
}

void LatticeTools::ConvertTextToLattice(const string& text, Lattice* pl) {
  Lattice& l = *pl;
  vector<WordID> ids;
  TD::ConvertSentence(text, &ids);
  l.clear();
  l.resize(ids.size());
  for (int i = 0; i < l.size(); ++i)
    l[i].push_back(LatticeArc(ids[i], 0.0, 1));
  l.is_sentence_ = true;
}

void LatticeTools::ConvertTextOrPLF(const string& text_or_plf, Lattice* pl) {
  if (LooksLikePLF(text_or_plf))
    HypergraphIO::PLFtoLattice(text_or_plf, pl);
  else
    ConvertTextToLattice(text_or_plf, pl);
  pl->ComputeDistances();
}

