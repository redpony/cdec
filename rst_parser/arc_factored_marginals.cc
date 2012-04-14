#include "arc_factored.h"

#include <iostream>

#include "config.h"

using namespace std;

#if HAVE_EIGEN

#include <Eigen/Dense>
typedef Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic> ArcMatrix;
typedef Eigen::Matrix<double, Eigen::Dynamic, 1> RootVector;

void ArcFactoredForest::EdgeMarginals(double *plog_z) {
  ArcMatrix A(num_words_,num_words_);
  RootVector r(num_words_);
  for (int h = 0; h < num_words_; ++h) {
    for (int m = 0; m < num_words_; ++m) {
      if (h != m)
        A(h,m) = edges_(h,m).edge_prob.as_float();
      else
        A(h,m) = 0;
    }
    r(h) = root_edges_[h].edge_prob.as_float();
  }

  ArcMatrix L = -A;
  L.diagonal() = A.colwise().sum();
  L.row(0) = r;
  ArcMatrix Linv = L.inverse();
  if (plog_z) *plog_z = log(Linv.determinant());
  RootVector rootMarginals = r.cwiseProduct(Linv.col(0));
  for (int h = 0; h < num_words_; ++h) {
    for (int m = 0; m < num_words_; ++m) {
      edges_(h,m).edge_prob = prob_t((m == 0 ? 0.0 : 1.0) * A(h,m) * Linv(m,m) -
                                     (h == 0 ? 0.0 : 1.0) * A(h,m) * Linv(m,h));
    }
    root_edges_[h].edge_prob = prob_t(rootMarginals(h));
  }
  // cerr << "ROOT MARGINALS: " << rootMarginals.transpose() << endl;
}

#else

void ArcFactoredForest::EdgeMarginals(double*) {
  cerr << "EdgeMarginals() requires --with-eigen!\n";
  abort();
}

#endif

