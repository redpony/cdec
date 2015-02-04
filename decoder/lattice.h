#ifndef LATTICE_H_
#define LATTICE_H_

#include <string>
#include <vector>
#include "sparse_vector.h"
#include "wordid.h"
#include "array2d.h"

class Lattice;
struct LatticeTools {
  static bool LooksLikePLF(const std::string &line);
  static void ConvertTextToLattice(const std::string& text, Lattice* pl);
  static void ConvertTextOrPLF(const std::string& text_or_plf, Lattice* pl);
};

struct LatticeArc {
  WordID label;
  SparseVector<double> features;
  int dist2next;
  LatticeArc() : label(), features(), dist2next() {}
  LatticeArc(WordID w, const SparseVector<double>& f, int i) : label(w), features(f), dist2next(i) {}
};

class Lattice : public std::vector<std::vector<LatticeArc> > {
  friend void LatticeTools::ConvertTextOrPLF(const std::string& text_or_plf, Lattice* pl);
  friend void LatticeTools::ConvertTextToLattice(const std::string& text, Lattice* pl);
 public:
  Lattice() {}
  explicit Lattice(size_t t, const std::vector<LatticeArc>& v = std::vector<LatticeArc>()) :
   std::vector<std::vector<LatticeArc>>(t, v) {}
  int Distance(int from, int to) const {
    if (dist_.empty())
      return (to - from);
    return dist_(from, to);
  }
 private:
  void ComputeDistances();
  Array2D<int> dist_;
};

inline bool IsSentence(const Lattice& in) {
  bool res = true;
  for (auto& alt : in)
    if (alt.size() > 1) { res = false; break; }
  return res;
}

#endif
