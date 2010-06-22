#ifndef __LATTICE_H_
#define __LATTICE_H_

#include <string>
#include <vector>
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
  double cost;
  int dist2next;
  LatticeArc() : label(), cost(), dist2next() {}
  LatticeArc(WordID w, double c, int i) : label(w), cost(c), dist2next(i) {}
};

class Lattice : public std::vector<std::vector<LatticeArc> > {
  friend void LatticeTools::ConvertTextOrPLF(const std::string& text_or_plf, Lattice* pl);
  friend void LatticeTools::ConvertTextToLattice(const std::string& text, Lattice* pl);
 public:
  Lattice() : is_sentence_(false) {}
  explicit Lattice(size_t t, const std::vector<LatticeArc>& v = std::vector<LatticeArc>()) :
   std::vector<std::vector<LatticeArc> >(t, v),
   is_sentence_(false) {}
  int Distance(int from, int to) const {
    if (dist_.empty())
      return (to - from);
    return dist_(from, to);
  }
  // TODO this should actually be computed based on the contents
  // of the lattice
  bool IsSentence() const { return is_sentence_; }
 private:
  void ComputeDistances();
  Array2D<int> dist_;
  bool is_sentence_;
};

#endif
