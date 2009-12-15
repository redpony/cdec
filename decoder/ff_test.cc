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
       
TEST_F(FFTest,LanguageModel) {
  vector<const FeatureFunction*> ms(1, lm_);
  TRulePtr tr1(new TRule("[X] ||| [X,1] said"));
  TRulePtr tr2(new TRule("[X] ||| the man said"));
  TRulePtr tr3(new TRule("[X] ||| the fat man"));  
  Hypergraph hg;
  const int lm_fid = FD::Convert("LanguageModel");
  vector<double> w(lm_fid + 1,1);
  ModelSet models(w, ms);
  string state;
  Hypergraph::Edge edge;
  edge.rule_ = tr2;
  models.AddFeaturesToEdge(smeta, hg, &edge, &state);
  double lm1 = edge.feature_values_.dot(w);
  cerr << "lm=" << edge.feature_values_[lm_fid] << endl;

  hg.nodes_.resize(1);
  hg.edges_.resize(2);
  hg.edges_[0].rule_ = tr3;
  models.AddFeaturesToEdge(smeta, hg, &hg.edges_[0], &hg.nodes_[0].state_);
  hg.edges_[1].tail_nodes_.push_back(0);
  hg.edges_[1].rule_ = tr1;
  string state2;
  models.AddFeaturesToEdge(smeta, hg, &hg.edges_[1], &state2);
  double tot = hg.edges_[1].feature_values_[lm_fid] + hg.edges_[0].feature_values_[lm_fid];
  cerr << "lm=" << tot << endl;
  EXPECT_TRUE(state2 == state);
  EXPECT_FALSE(state == hg.nodes_[0].state_);
}

TEST_F(FFTest, Small) {
  WordPenalty wp("");
  vector<const FeatureFunction*> ms(2, lm_);
  ms[1] = &wp;
  TRulePtr tr1(new TRule("[X] ||| [X,1] said"));
  TRulePtr tr2(new TRule("[X] ||| john said"));
  TRulePtr tr3(new TRule("[X] ||| john"));
  cerr << "RULE: " << tr1->AsString() << endl;
  Hypergraph hg;
  vector<double> w(2); w[0]=1.0; w[1]=-2.0;
  ModelSet models(w, ms);
  string state;
  Hypergraph::Edge edge;
  edge.rule_ = tr2;
  cerr << tr2->AsString() << endl;
  models.AddFeaturesToEdge(smeta, hg, &edge, &state);
  double s1 = edge.feature_values_.dot(w);
  cerr << "lm=" << edge.feature_values_[0] << endl;
  cerr << "wp=" << edge.feature_values_[1] << endl;

  hg.nodes_.resize(1);
  hg.edges_.resize(2);
  hg.edges_[0].rule_ = tr3;
  models.AddFeaturesToEdge(smeta, hg, &hg.edges_[0], &hg.nodes_[0].state_);
  double acc = hg.edges_[0].feature_values_.dot(w);
  cerr << hg.edges_[0].feature_values_[0] << endl;
  hg.edges_[1].tail_nodes_.push_back(0);
  hg.edges_[1].rule_ = tr1;
  string state2;
  models.AddFeaturesToEdge(smeta, hg, &hg.edges_[1], &state2);
  acc += hg.edges_[1].feature_values_.dot(w);
  double tot = hg.edges_[1].feature_values_[0] + hg.edges_[0].feature_values_[0];
  cerr << "lm=" << tot << endl;
  cerr << "acc=" << acc << endl;
  cerr << " s1=" << s1 << endl;
  EXPECT_TRUE(state2 == state);
  EXPECT_FALSE(state == hg.nodes_[0].state_);
  EXPECT_FLOAT_EQ(acc, s1);
}

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
