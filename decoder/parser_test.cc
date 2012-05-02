#define BOOST_TEST_MODULE ParseTest
#include <boost/test/unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp>
#include "lattice.h"
#include "hg.h"
#include "trule.h"
#include "bottom_up_parser.h"
#include "tdict.h"

using namespace std;

BOOST_AUTO_TEST_CASE(Parse) {
  LatticeArc a(TD::Convert("ein"), 0.0, 1);
  LatticeArc b(TD::Convert("haus"), 0.0, 1);
  Lattice lattice(2);
  lattice[0].push_back(a);
  lattice[1].push_back(b);
  Hypergraph forest;
  GrammarPtr g(new TextGrammar);
  vector<GrammarPtr> grammars(1, g);
  ExhaustiveBottomUpParser parser("PHRASE", grammars);
  parser.Parse(lattice, &forest);
}

