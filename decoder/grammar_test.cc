#define BOOST_TEST_MODULE g_test
#include <boost/test/unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp>

#include <cassert>
#include <iostream>
#include <fstream>
#include <vector>
#include "trule.h"
#include "tdict.h"
#include "grammar.h"
#include "bottom_up_parser.h"
#include "hg.h"
#include "ff.h"
#include "ffset.h"
#include "weights.h"

using namespace std;

struct GrammarTest {
  GrammarTest() {
    std::string path(boost::unit_test::framework::master_test_suite().argc == 2 ? boost::unit_test::framework::master_test_suite().argv[1] : TEST_DATA);
    Weights::InitFromFile(path + "/weights.gt", &wts);
  }
  vector<weight_t> wts;
};

BOOST_FIXTURE_TEST_SUITE( s, GrammarTest );

BOOST_AUTO_TEST_CASE(TestTextGrammar) {
  vector<double> w;
  vector<const FeatureFunction*> ms;
  ModelSet models(w, ms);

  TextGrammar g;
  TRulePtr r1(new TRule("[X] ||| a b c ||| A B C ||| 0.1 0.2 0.3", true));
  TRulePtr r2(new TRule("[X] ||| a b c ||| 1 2 3 ||| 0.2 0.3 0.4", true));
  TRulePtr r3(new TRule("[X] ||| a b c d ||| A B C D ||| 0.1 0.2 0.3", true));
  cerr << r1->AsString() << endl;
  g.AddRule(r1);
  g.AddRule(r2);
  g.AddRule(r3);
}

BOOST_AUTO_TEST_CASE(TestTextGrammarFile) {
  std::string path(boost::unit_test::framework::master_test_suite().argc == 2 ? boost::unit_test::framework::master_test_suite().argv[1] : TEST_DATA);
  GrammarPtr g(new TextGrammar(path + "/grammar.prune"));
  vector<GrammarPtr> grammars(1, g);

  LatticeArc a(TD::Convert("ein"), 0.0, 1);
  LatticeArc b(TD::Convert("haus"), 0.0, 1);
  Lattice lattice(2);
  lattice[0].push_back(a);
  lattice[1].push_back(b);
  Hypergraph forest;
  ExhaustiveBottomUpParser parser("PHRASE", grammars);
  parser.Parse(lattice, &forest);
  forest.PrintGraphviz();
}
BOOST_AUTO_TEST_SUITE_END()

