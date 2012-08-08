#include "arc_factored.h"

#include <vector>
#include <iostream>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "timing_stats.h"
#include "arc_ff.h"
#include "dep_training.h"
#include "stringlib.h"
#include "filelib.h"
#include "tdict.h"
#include "weights.h"
#include "rst.h"
#include "global_ff.h"

using namespace std;
namespace po = boost::program_options;

int main(int argc, char** argv) {
  if (argc != 2) {
    cerr << argv[0] << " N\n" << endl;
    return 1;
  }
  MT19937 rng;
  unsigned n = atoi(argv[1]);

  ArcFactoredForest forest(n);
  TreeSampler ts(forest);
  EdgeSubset tree;
  ts.SampleRandomSpanningTree(&tree, &rng);
  cout << tree << endl;
  return 0;
}

