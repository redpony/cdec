#include <cassert>
#include <iostream>
#include <fstream>
#include <vector>
#include <gtest/gtest.h>
#include "hg.h"
#include "trule.h"
#include "bottom_up_parser.h"
#include "tdict.h"

using namespace std;

class ChartTest : public testing::Test {
 protected:
  virtual void SetUp() { }
  virtual void TearDown() { }
};
       
TEST_F(ChartTest,LanguageModel) {
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

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
