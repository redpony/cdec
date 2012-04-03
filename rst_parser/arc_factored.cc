#include "arc_factored.h"

#include <set>

#include <boost/pending/disjoint_sets.hpp>

using namespace std;
using namespace boost;

// based on Trajan 1977
void ArcFactoredForest::MaximumSpanningTree(SpanningTree* st) const {
  typedef disjoint_sets_with_storage<identity_property_map, identity_property_map,
      find_with_full_path_compression> DisjointSet;
  DisjointSet strongly(num_words_ + 1);
  DisjointSet weakly(num_words_ + 1);
  set<unsigned> roots, h, rset;
  vector<pair<short, short> > enter(num_words_ + 1);
  for (unsigned i = 0; i <= num_words_; ++i) {
    strongly.make_set(i);
    weakly.make_set(i);
    roots.insert(i);
  }
  while(!roots.empty()) {
    set<unsigned>::iterator it = roots.begin();
    const unsigned k = *it;
    roots.erase(it);
    cerr << "k=" << k << endl;
    pair<short,short> ij; // TODO = Max(k);
  }
}

