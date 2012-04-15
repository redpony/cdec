#include "arc_factored.h"

#include <iostream>

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
  double lz;
  af.EdgeMarginals(&lz);
  cerr << "Z = " << lz << endl;
  af.PickBestParentForEachWord(&tree);
  cerr << tree << endl;
  return 0;
}

