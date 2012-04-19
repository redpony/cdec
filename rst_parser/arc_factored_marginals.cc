#include "arc_factored.h"

#include <iostream>

#include "config.h"

using namespace std;

#if HAVE_EIGEN

#include <Eigen/Dense>
typedef Eigen::Matrix<prob_t, Eigen::Dynamic, Eigen::Dynamic> ArcMatrix;
typedef Eigen::Matrix<prob_t, Eigen::Dynamic, 1> RootVector;

void ArcFactoredForest::EdgeMarginals(prob_t *plog_z) {
  ArcMatrix A(num_words_,num_words_);
  RootVector r(num_words_);
  for (int h = 0; h < num_words_; ++h) {
    for (int m = 0; m < num_words_; ++m) {
      if (h != m)
        A(h,m) = edges_(h,m).edge_prob;
      else
        A(h,m) = prob_t::Zero();
    }
    r(h) = root_edges_[h].edge_prob;
  }

  ArcMatrix L = -A;
  L.diagonal() = A.colwise().sum();
  L.row(0) = r;
  ArcMatrix Linv = L.inverse();
  if (plog_z) *plog_z = Linv.determinant();
  RootVector rootMarginals = r.cwiseProduct(Linv.col(0));
  static const prob_t ZERO(0);
  static const prob_t ONE(1);
//  ArcMatrix T = Linv;
  for (int h = 0; h < num_words_; ++h) {
    for (int m = 0; m < num_words_; ++m) {
      const prob_t marginal = (m == 0 ? ZERO : ONE) * A(h,m) * Linv(m,m) -
                              (h == 0 ? ZERO : ONE) * A(h,m) * Linv(m,h);
      edges_(h,m).edge_prob = marginal;
//      T(h,m) = marginal;
    }
    root_edges_[h].edge_prob = rootMarginals(h);
  }
//   cerr << "ROOT MARGINALS: " << rootMarginals.transpose() << endl;
//  cerr << "M:\n" << T << endl;
}

#else

void ArcFactoredForest::EdgeMarginals(prob_t *) {
  cerr << "EdgeMarginals() requires --with-eigen!\n";
  abort();
}

#endif

