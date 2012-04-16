#include "arc_factored.h"

#include <iostream>

#include <Eigen/Dense>

using namespace std;

int main(int argc, char** argv) {
  // John saw Mary
  //   (H -> M)
  //   (1 -> 2) 20
  //   (1 -> 3) 3
  //   (2 -> 1) 20
  //   (2 -> 3) 30
  //   (3 -> 2) 0
  //   (3 -> 1) 11
  //   (0, 2) 10
  //   (0, 1) 9
  //   (0, 3) 9
  ArcFactoredForest af(3);
  af(0,1).edge_prob.logeq(20);
  af(0,2).edge_prob.logeq(3);
  af(1,0).edge_prob.logeq(20);
  af(1,2).edge_prob.logeq(30);
  af(2,1).edge_prob.logeq(0);
  af(2,0).edge_prob.logeq(11);
  af(-1,1).edge_prob.logeq(10);
  af(-1,0).edge_prob.logeq(9);
  af(-1,2).edge_prob.logeq(9);
  EdgeSubset tree;
//  af.MaximumEdgeSubset(&tree);
  prob_t z;
  af.EdgeMarginals(&z);
  cerr << "Z = " << abs(z) << endl;
  af.PickBestParentForEachWord(&tree);
  cerr << tree << endl;
  typedef Eigen::Matrix<prob_t, 2, 2> M3;
  M3 A = M3::Zero();
  A(0,0) = prob_t(1);
  A(1,0) = prob_t(3);
  A(0,1) = prob_t(2);
  A(1,1) = prob_t(4);
  prob_t det = A.determinant();
  cerr << det.as_float() << endl;
  return 0;
}

