#include <cassert>
#include <iostream>
#include <fstream>
#include <vector>
#include <gtest/gtest.h>
#include "hg.h"
#include "ff_lm.h"
#include "ff.h"
#include "trule.h"
#include "sentence_metadata.h"

using namespace std;

LanguageModel* lm_ = NULL;
LanguageModel* lm3_ = NULL;

class FFTest : public testing::Test {
 public:
  FFTest() : smeta(0,Lattice()) {
    if (!lm_) {
      static LanguageModel slm("-o 2 ./test_data/test_2gram.lm.gz");
      lm_ = &slm;
      static LanguageModel slm3("./test_data/dummy.3gram.lm -o 3");
      lm3_ = &slm3;
    }
  }
 protected:
  virtual void SetUp() { }
  virtual void TearDown() { }
  SentenceMetadata smeta;
};
       
TEST_F(FFTest, LM3) {
  int x = lm3_->NumBytesContext();
  Hypergraph::Edge edge1;
  edge1.rule_.reset(new TRule("[X] ||| x y ||| one ||| 1.0 -2.4 3.0"));
  Hypergraph::Edge edge2;
  edge2.rule_.reset(new TRule("[X] ||| [X,1] a ||| [X,1] two ||| 1.0 -2.4 3.0"));
  Hypergraph::Edge edge3;
  edge3.rule_.reset(new TRule("[X] ||| [X,1] a ||| zero [X,1] two ||| 1.0 -2.4 3.0"));
  vector<const void*> ants1;
  string state(x, '\0');
  SparseVector<double> feats;
  SparseVector<double> est;
  lm3_->TraversalFeatures(smeta, edge1, ants1, &feats, &est, (void *)&state[0]);
  cerr << "returned " << feats << endl;
  cerr << edge1.feature_values_ << endl;
  cerr << lm3_->DebugStateToString((const void*)&state[0]) << endl;
  EXPECT_EQ("[ one ]", lm3_->DebugStateToString((const void*)&state[0]));
  ants1.push_back((const void*)&state[0]);
  string state2(x, '\0');
  lm3_->TraversalFeatures(smeta, edge2, ants1, &feats, &est, (void *)&state2[0]);
  cerr << lm3_->DebugStateToString((const void*)&state2[0]) << endl;
  EXPECT_EQ("[ one two ]", lm3_->DebugStateToString((const void*)&state2[0]));
  string state3(x, '\0');
  lm3_->TraversalFeatures(smeta, edge3, ants1, &feats, &est, (void *)&state3[0]);
  cerr << lm3_->DebugStateToString((const void*)&state3[0]) << endl;
  EXPECT_EQ("[ zero one <{STAR}> one two ]", lm3_->DebugStateToString((const void*)&state3[0]));
}

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
