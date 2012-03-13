#include "sparse_vector.h"

namespace Mira {
  class Hildreth {
  public :
    static std::vector<double> optimise(std::vector< SparseVector<double> >& a, std::vector<double>& b);
    static std::vector<double> optimise(std::vector< SparseVector<double> >& a, std::vector<double>& b, double C);
  };
}

