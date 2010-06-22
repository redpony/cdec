#include <cassert>
#include <iostream>
#include <fstream>
#include <vector>
#include <gtest/gtest.h>
#include "trule.h"
#include "tdict.h"
#include "grammar.h"
#include "bottom_up_parser.h"
#include "ff.h"
#include "weights.h"

using namespace std;

class GrammarTest : public testing::Test {
 public:
  GrammarTest() {
    wts.InitFromFile("test_data/weights.gt");
  }
 protected:
  virtual void SetUp() { }
  virtual void TearDown() { }
  Weights wts;
};
       
TEST_F(GrammarTest,TestTextGrammar) {
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

TEST_F(GrammarTest,TestTextGrammarFile) {
  GrammarPtr g(new TextGrammar("./test_data/grammar.prune"));
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

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
