#include <cassert>
#include <iostream>
#include <fstream>
#include <vector>
#include <gtest/gtest.h>
#include "tdict.h"

#include "json_parse.h"
#include "filelib.h"
#include "hg.h"
#include "hg_io.h"
#include "hg_intersect.h"
#include "viterbi.h"
#include "kbest.h"
#include "inside_outside.h"

using namespace std;

class HGTest : public testing::Test {
 protected:
  virtual void SetUp() { }
  virtual void TearDown() { }
  void CreateHG(Hypergraph* hg) const;
  void CreateHG_int(Hypergraph* hg) const;
  void CreateHG_tiny(Hypergraph* hg) const;
  void CreateHGBalanced(Hypergraph* hg) const;
  void CreateLatticeHG(Hypergraph* hg) const;
  void CreateTinyLatticeHG(Hypergraph* hg) const;
};
       
void HGTest::CreateTinyLatticeHG(Hypergraph* hg) const {
  const string json = "{\"rules\":[1,\"[X] ||| [1] a\",2,\"[X] ||| [1] A\",3,\"[X] ||| [1] b\",4,\"[X] ||| [1] B'\"],\"features\":[\"f1\",\"f2\",\"Feature_1\",\"Feature_0\",\"Model_0\",\"Model_1\",\"Model_2\",\"Model_3\",\"Model_4\",\"Model_5\",\"Model_6\",\"Model_7\"],\"edges\":[],\"node\":{\"in_edges\":[]},\"edges\":[{\"tail\":[0],\"feats\":[0,-0.2],\"rule\":1},{\"tail\":[0],\"feats\":[0,-0.6],\"rule\":2}],\"node\":{\"in_edges\":[0,1]},\"edges\":[{\"tail\":[1],\"feats\":[0,-0.1],\"rule\":3},{\"tail\":[1],\"feats\":[0,-0.9],\"rule\":4}],\"node\":{\"in_edges\":[2,3]}}";
  istringstream instr(json);
  EXPECT_TRUE(HypergraphIO::ReadFromJSON(&instr, hg));
}

void HGTest::CreateLatticeHG(Hypergraph* hg) const {
  const string json = "{\"rules\":[1,\"[X] ||| [1] a\",2,\"[X] ||| [1] A\",3,\"[X] ||| [1] A A\",4,\"[X] ||| [1] b\",5,\"[X] ||| [1] c\",6,\"[X] ||| [1] B C\",7,\"[X] ||| [1] A B C\",8,\"[X] ||| [1] CC\"],\"features\":[\"f1\",\"f2\",\"Feature_1\",\"Feature_0\",\"Model_0\",\"Model_1\",\"Model_2\",\"Model_3\",\"Model_4\",\"Model_5\",\"Model_6\",\"Model_7\"],\"edges\":[],\"node\":{\"in_edges\":[]},\"edges\":[{\"tail\":[0],\"feats\":[2,-0.3],\"rule\":1},{\"tail\":[0],\"feats\":[2,-0.6],\"rule\":2},{\"tail\":[0],\"feats\":[2,-1.7],\"rule\":3}],\"node\":{\"in_edges\":[0,1,2]},\"edges\":[{\"tail\":[1],\"feats\":[2,-0.5],\"rule\":4}],\"node\":{\"in_edges\":[3]},\"edges\":[{\"tail\":[2],\"feats\":[2,-0.6],\"rule\":5},{\"tail\":[1],\"feats\":[2,-0.8],\"rule\":6},{\"tail\":[0],\"feats\":[2,-0.01],\"rule\":7},{\"tail\":[2],\"feats\":[2,-0.8],\"rule\":8}],\"node\":{\"in_edges\":[4,5,6,7]}}";
  istringstream instr(json);
  EXPECT_TRUE(HypergraphIO::ReadFromJSON(&instr, hg));
}

void HGTest::CreateHG_tiny(Hypergraph* hg) const {
  const string json = "{\"rules\":[1,\"[X] ||| <s>\",2,\"[X] ||| X [1]\",3,\"[X] ||| Z [1]\"],\"features\":[\"f1\",\"f2\",\"Feature_1\",\"Feature_0\",\"Model_0\",\"Model_1\",\"Model_2\",\"Model_3\",\"Model_4\",\"Model_5\",\"Model_6\",\"Model_7\"],\"edges\":[{\"tail\":[],\"feats\":[0,-2,1,-99],\"rule\":1}],\"node\":{\"in_edges\":[0]},\"edges\":[{\"tail\":[0],\"feats\":[0,-0.5,1,-0.8],\"rule\":2},{\"tail\":[0],\"feats\":[0,-0.7,1,-0.9],\"rule\":3}],\"node\":{\"in_edges\":[1,2]}}";
  istringstream instr(json);
  EXPECT_TRUE(HypergraphIO::ReadFromJSON(&instr, hg));
}

void HGTest::CreateHG_int(Hypergraph* hg) const {
  const string json = "{\"rules\":[1,\"[X] ||| a\",2,\"[X] ||| b\",3,\"[X] ||| a [1]\",4,\"[X] ||| [1] b\"],\"features\":[\"f1\",\"f2\",\"Feature_1\",\"Feature_0\",\"Model_0\",\"Model_1\",\"Model_2\",\"Model_3\",\"Model_4\",\"Model_5\",\"Model_6\",\"Model_7\"],\"edges\":[{\"tail\":[],\"feats\":[0,0.1],\"rule\":1},{\"tail\":[],\"feats\":[0,0.1],\"rule\":2}],\"node\":{\"in_edges\":[0,1],\"cat\":\"X\"},\"edges\":[{\"tail\":[0],\"feats\":[0,0.3],\"rule\":3},{\"tail\":[0],\"feats\":[0,0.2],\"rule\":4}],\"node\":{\"in_edges\":[2,3],\"cat\":\"Goal\"}}";
  istringstream instr(json);
  EXPECT_TRUE(HypergraphIO::ReadFromJSON(&instr, hg));
}

void HGTest::CreateHG(Hypergraph* hg) const {
  string json = "{\"rules\":[1,\"[X] ||| a\",2,\"[X] ||| A [1]\",3,\"[X] ||| c\",4,\"[X] ||| C [1]\",5,\"[X] ||| [1] B [2]\",6,\"[X] ||| [1] b [2]\",7,\"[X] ||| X [1]\",8,\"[X] ||| Z [1]\"],\"features\":[\"f1\",\"f2\",\"Feature_1\",\"Feature_0\",\"Model_0\",\"Model_1\",\"Model_2\",\"Model_3\",\"Model_4\",\"Model_5\",\"Model_6\",\"Model_7\"],\"edges\":[{\"tail\":[],\"feats\":[],\"rule\":1}],\"node\":{\"in_edges\":[0]},\"edges\":[{\"tail\":[0],\"feats\":[0,-0.8,1,-0.1],\"rule\":2}],\"node\":{\"in_edges\":[1]},\"edges\":[{\"tail\":[],\"feats\":[1,-1],\"rule\":3}],\"node\":{\"in_edges\":[2]},\"edges\":[{\"tail\":[2],\"feats\":[0,-0.2,1,-0.1],\"rule\":4}],\"node\":{\"in_edges\":[3]},\"edges\":[{\"tail\":[1,3],\"feats\":[0,-1.2,1,-0.2],\"rule\":5},{\"tail\":[1,3],\"feats\":[0,-0.5,1,-1.3],\"rule\":6}],\"node\":{\"in_edges\":[4,5]},\"edges\":[{\"tail\":[4],\"feats\":[0,-0.5,1,-0.8],\"rule\":7},{\"tail\":[4],\"feats\":[0,-0.7,1,-0.9],\"rule\":8}],\"node\":{\"in_edges\":[6,7]}}";
  istringstream instr(json);
  EXPECT_TRUE(HypergraphIO::ReadFromJSON(&instr, hg));
}

void HGTest::CreateHGBalanced(Hypergraph* hg) const {
  const string json = "{\"rules\":[1,\"[X] ||| i\",2,\"[X] ||| a\",3,\"[X] ||| b\",4,\"[X] ||| [1] [2]\",5,\"[X] ||| [1] [2]\",6,\"[X] ||| c\",7,\"[X] ||| d\",8,\"[X] ||| [1] [2]\",9,\"[X] ||| [1] [2]\",10,\"[X] ||| [1] [2]\",11,\"[X] ||| [1] [2]\",12,\"[X] ||| [1] [2]\",13,\"[X] ||| [1] [2]\"],\"features\":[\"f1\",\"f2\",\"Feature_1\",\"Feature_0\",\"Model_0\",\"Model_1\",\"Model_2\",\"Model_3\",\"Model_4\",\"Model_5\",\"Model_6\",\"Model_7\"],\"edges\":[{\"tail\":[],\"feats\":[],\"rule\":1}],\"node\":{\"in_edges\":[0]},\"edges\":[{\"tail\":[],\"feats\":[],\"rule\":2}],\"node\":{\"in_edges\":[1]},\"edges\":[{\"tail\":[],\"feats\":[],\"rule\":3}],\"node\":{\"in_edges\":[2]},\"edges\":[{\"tail\":[1,2],\"feats\":[],\"rule\":4},{\"tail\":[2,1],\"feats\":[],\"rule\":5}],\"node\":{\"in_edges\":[3,4]},\"edges\":[{\"tail\":[],\"feats\":[],\"rule\":6}],\"node\":{\"in_edges\":[5]},\"edges\":[{\"tail\":[],\"feats\":[],\"rule\":7}],\"node\":{\"in_edges\":[6]},\"edges\":[{\"tail\":[4,5],\"feats\":[],\"rule\":8},{\"tail\":[5,4],\"feats\":[],\"rule\":9}],\"node\":{\"in_edges\":[7,8]},\"edges\":[{\"tail\":[3,6],\"feats\":[],\"rule\":10},{\"tail\":[6,3],\"feats\":[],\"rule\":11}],\"node\":{\"in_edges\":[9,10]},\"edges\":[{\"tail\":[7,0],\"feats\":[],\"rule\":12},{\"tail\":[0,7],\"feats\":[],\"rule\":13}],\"node\":{\"in_edges\":[11,12]}}";
  istringstream instr(json);
  EXPECT_TRUE(HypergraphIO::ReadFromJSON(&instr, hg));
}

TEST_F(HGTest,Controlled) {
  Hypergraph hg;
  CreateHG_tiny(&hg);
  SparseVector<double> wts;
  wts.set_value(FD::Convert("f1"), 0.4);
  wts.set_value(FD::Convert("f2"), 0.8);
  hg.Reweight(wts);
  vector<WordID> trans;
  prob_t prob = ViterbiESentence(hg, &trans);
  cerr << TD::GetString(trans) << "\n";
  cerr << "prob: " << prob << "\n";
  EXPECT_FLOAT_EQ(-80.839996, log(prob));
  EXPECT_EQ("X <s>", TD::GetString(trans));
  vector<prob_t> post;
  hg.PrintGraphviz();
  prob_t c2 = Inside<prob_t, ScaledEdgeProb>(hg, NULL, ScaledEdgeProb(0.6));
  EXPECT_FLOAT_EQ(-47.8577, log(c2));
}

TEST_F(HGTest,Union) {
  Hypergraph hg1;
  Hypergraph hg2;
  CreateHG_tiny(&hg1);
  CreateHG(&hg2);
  SparseVector<double> wts;
  wts.set_value(FD::Convert("f1"), 0.4);
  wts.set_value(FD::Convert("f2"), 1.0);
  hg1.Reweight(wts);
  hg2.Reweight(wts);
  prob_t c1,c2,c3,c4;
  vector<WordID> t1,t2,t3,t4;
  c1 = ViterbiESentence(hg1, &t1);
  c2 = ViterbiESentence(hg2, &t2);
  int l2 = ViterbiPathLength(hg2);
  cerr << c1 << "\t" << TD::GetString(t1) << endl;
  cerr << c2 << "\t" << TD::GetString(t2) << endl;
  hg1.Union(hg2);
  hg1.Reweight(wts);
  c3 = ViterbiESentence(hg1, &t3);
  int l3 = ViterbiPathLength(hg1);
  cerr << c3 << "\t" << TD::GetString(t3) << endl;
  EXPECT_FLOAT_EQ(c2, c3);
  EXPECT_EQ(TD::GetString(t2), TD::GetString(t3));
  EXPECT_EQ(l2, l3);

  wts.set_value(FD::Convert("f2"), -1);
  hg1.Reweight(wts);
  c4 = ViterbiESentence(hg1, &t4);
  cerr << c4 << "\t" << TD::GetString(t4) << endl;
  EXPECT_EQ("Z <s>", TD::GetString(t4));
  EXPECT_FLOAT_EQ(98.82, log(c4));

  vector<pair<vector<WordID>, prob_t> > list;
  KBest::KBestDerivations<vector<WordID>, ESentenceTraversal> kbest(hg1, 10);
  for (int i = 0; i < 10; ++i) {
    const KBest::KBestDerivations<vector<WordID>, ESentenceTraversal>::Derivation* d =
      kbest.LazyKthBest(hg1.nodes_.size() - 1, i);
    if (!d) break;
    list.push_back(make_pair(d->yield, d->score));
  }
  EXPECT_TRUE(list[0].first == t4);
  EXPECT_FLOAT_EQ(log(list[0].second), log(c4));
  EXPECT_EQ(list.size(), 6);
  EXPECT_FLOAT_EQ(log(list.back().second / list.front().second), -97.7);
}

TEST_F(HGTest,ControlledKBest) {
  Hypergraph hg;
  CreateHG(&hg);
  vector<double> w(2); w[0]=0.4; w[1]=0.8;
  hg.Reweight(w);
  vector<WordID> trans;
  prob_t cost = ViterbiESentence(hg, &trans);
  cerr << TD::GetString(trans) << "\n";
  cerr << "cost: " << cost << "\n";

  int best = 0;
  KBest::KBestDerivations<vector<WordID>, ESentenceTraversal> kbest(hg, 10);
  for (int i = 0; i < 10; ++i) {
    const KBest::KBestDerivations<vector<WordID>, ESentenceTraversal>::Derivation* d =
      kbest.LazyKthBest(hg.nodes_.size() - 1, i);
    if (!d) break;
    cerr << TD::GetString(d->yield) << endl;
    ++best;
  }
  EXPECT_EQ(4, best);
}


TEST_F(HGTest,InsideScore) {
  SparseVector<double> wts;
  wts.set_value(FD::Convert("f1"), 1.0);
  Hypergraph hg;
  CreateTinyLatticeHG(&hg);
  hg.Reweight(wts);
  vector<WordID> trans;
  prob_t cost = ViterbiESentence(hg, &trans);
  cerr << TD::GetString(trans) << "\n";
  cerr << "cost: " << cost << "\n";
  hg.PrintGraphviz();
  prob_t inside = Inside<prob_t, EdgeProb>(hg);
  EXPECT_FLOAT_EQ(1.7934048, inside);  // computed by hand
  vector<prob_t> post;
  inside = hg.ComputeBestPathThroughEdges(&post);
  EXPECT_FLOAT_EQ(-0.3, log(inside));  // computed by hand
  EXPECT_EQ(post.size(), 4);
  for (int i = 0; i < 4; ++i) {
    cerr << "edge post: " << log(post[i]) << '\t' << hg.edges_[i].rule_->AsString() << endl;
  }
}


TEST_F(HGTest,PruneInsideOutside) {
  SparseVector<double> wts;
  wts.set_value(FD::Convert("Feature_1"), 1.0);
  Hypergraph hg;
  CreateLatticeHG(&hg);
  hg.Reweight(wts);
  vector<WordID> trans;
  prob_t cost = ViterbiESentence(hg, &trans);
  cerr << TD::GetString(trans) << "\n";
  cerr << "cost: " << cost << "\n";
  hg.PrintGraphviz();
  //hg.DensityPruneInsideOutside(0.5, false, 2.0);
  hg.BeamPruneInsideOutside(0.5, false, 0.5);
  cost = ViterbiESentence(hg, &trans);
  cerr << "Ncst: " << cost << endl;
  cerr << TD::GetString(trans) << "\n";
  hg.PrintGraphviz();
}

TEST_F(HGTest,TestPruneEdges) {
  Hypergraph hg;
  CreateLatticeHG(&hg);
  SparseVector<double> wts;
  wts.set_value(FD::Convert("f1"), 1.0);
  hg.Reweight(wts);
  hg.PrintGraphviz();
  vector<bool> prune(hg.edges_.size(), true);
  prune[6] = false;
  hg.PruneEdges(prune);
  cerr << "Pruned:\n";
  hg.PrintGraphviz();
}

TEST_F(HGTest,TestIntersect) {
  Hypergraph hg;
  CreateHG_int(&hg);
  SparseVector<double> wts;
  wts.set_value(FD::Convert("f1"), 1.0);
  hg.Reweight(wts);
  hg.PrintGraphviz();

  int best = 0;
  KBest::KBestDerivations<vector<WordID>, ESentenceTraversal> kbest(hg, 10);
  for (int i = 0; i < 10; ++i) {
    const KBest::KBestDerivations<vector<WordID>, ESentenceTraversal>::Derivation* d =
      kbest.LazyKthBest(hg.nodes_.size() - 1, i);
    if (!d) break;
    cerr << TD::GetString(d->yield) << endl;
    ++best;
  }
  EXPECT_EQ(4, best);

  Lattice target(2);
  target[0].push_back(LatticeArc(TD::Convert("a"), 0.0, 1));
  target[1].push_back(LatticeArc(TD::Convert("b"), 0.0, 1));
  HG::Intersect(target, &hg);
  hg.PrintGraphviz();
}

TEST_F(HGTest,TestPrune2) {
  Hypergraph hg;
  CreateHG_int(&hg);
  SparseVector<double> wts;
  wts.set_value(FD::Convert("f1"), 1.0);
  hg.Reweight(wts);
  hg.PrintGraphviz();
  vector<bool> rem(hg.edges_.size(), false);
  rem[0] = true;
  rem[1] = true;
  hg.PruneEdges(rem);
  hg.PrintGraphviz();
  cerr << "TODO: fix this pruning behavior-- the resulting HG should be empty!\n";
}

TEST_F(HGTest,Sample) {
  Hypergraph hg;
  CreateLatticeHG(&hg);
  SparseVector<double> wts;
  wts.set_value(FD::Convert("Feature_1"), 0.0);
  hg.Reweight(wts);
  vector<WordID> trans;
  prob_t cost = ViterbiESentence(hg, &trans);
  cerr << TD::GetString(trans) << "\n";
  cerr << "cost: " << cost << "\n";
  hg.PrintGraphviz();
}

TEST_F(HGTest,PLF) {
  Hypergraph hg;
  string inplf = "((('haupt',-2.06655,1),('hauptgrund',-5.71033,2),),(('grund',-1.78709,1),),(('für\\'',0.1,1),),)";
  HypergraphIO::ReadFromPLF(inplf, &hg);
  SparseVector<double> wts;
  wts.set_value(FD::Convert("Feature_0"), 1.0);
  hg.Reweight(wts);
  hg.PrintGraphviz();
  string outplf = HypergraphIO::AsPLF(hg);
  cerr << " IN: " << inplf << endl;
  cerr << "OUT: " << outplf << endl;
  assert(inplf == outplf);
}

TEST_F(HGTest,PushWeightsToGoal) {
  Hypergraph hg;
  CreateHG(&hg);
  vector<double> w(2); w[0]=0.4; w[1]=0.8;
  hg.Reweight(w);
  vector<WordID> trans;
  prob_t cost = ViterbiESentence(hg, &trans);
  cerr << TD::GetString(trans) << "\n";
  cerr << "cost: " << cost << "\n";
  hg.PrintGraphviz();
  hg.PushWeightsToGoal();
  hg.PrintGraphviz();
}

TEST_F(HGTest,TestSpecialKBest) {
  Hypergraph hg;
  CreateHGBalanced(&hg);
  vector<double> w(1); w[0]=0;
  hg.Reweight(w);
  vector<pair<vector<WordID>, prob_t> > list;
  KBest::KBestDerivations<vector<WordID>, ESentenceTraversal> kbest(hg, 100000);
  for (int i = 0; i < 100000; ++i) {
    const KBest::KBestDerivations<vector<WordID>, ESentenceTraversal>::Derivation* d =
      kbest.LazyKthBest(hg.nodes_.size() - 1, i);
    if (!d) break;
    cerr << TD::GetString(d->yield) << endl;
  }
  hg.PrintGraphviz();
}

TEST_F(HGTest, TestGenericViterbi) {
  Hypergraph hg;
  CreateHG_tiny(&hg);
  SparseVector<double> wts;
  wts.set_value(FD::Convert("f1"), 0.4);
  wts.set_value(FD::Convert("f2"), 0.8);
  hg.Reweight(wts);
  vector<WordID> trans;
  const prob_t prob = ViterbiESentence(hg, &trans);
  cerr << TD::GetString(trans) << "\n";
  cerr << "prob: " << prob << "\n";
  EXPECT_FLOAT_EQ(-80.839996, log(prob));
  EXPECT_EQ("X <s>", TD::GetString(trans));
}

TEST_F(HGTest, TestGenericInside) {
  Hypergraph hg;
  CreateTinyLatticeHG(&hg);
  SparseVector<double> wts;
  wts.set_value(FD::Convert("f1"), 1.0);
  hg.Reweight(wts);
  vector<prob_t> inside;
  prob_t ins = Inside<prob_t, EdgeProb>(hg, &inside);
  EXPECT_FLOAT_EQ(1.7934048, ins);  // computed by hand
  vector<prob_t> outside;
  Outside<prob_t, EdgeProb>(hg, inside, &outside);
  EXPECT_EQ(3, outside.size());
  EXPECT_FLOAT_EQ(1.7934048, outside[0]);
  EXPECT_FLOAT_EQ(1.3114071, outside[1]);
  EXPECT_FLOAT_EQ(1.0, outside[2]);
}

TEST_F(HGTest,TestGenericInside2) {
  Hypergraph hg;
  CreateHG(&hg);
  SparseVector<double> wts;
  wts.set_value(FD::Convert("f1"), 0.4);
  wts.set_value(FD::Convert("f2"), 0.8);
  hg.Reweight(wts);
  vector<prob_t> inside, outside;
  prob_t ins = Inside<prob_t, EdgeProb>(hg, &inside);
  Outside<prob_t, EdgeProb>(hg, inside, &outside);
  for (int i = 0; i < hg.nodes_.size(); ++i)
    cerr << i << "\t" << log(inside[i]) << "\t" << log(outside[i]) << endl;
  EXPECT_FLOAT_EQ(0, log(inside[0]));
  EXPECT_FLOAT_EQ(-1.7861683, log(outside[0]));
  EXPECT_FLOAT_EQ(-0.4, log(inside[1]));
  EXPECT_FLOAT_EQ(-1.3861683, log(outside[1]));
  EXPECT_FLOAT_EQ(-0.8, log(inside[2]));
  EXPECT_FLOAT_EQ(-0.986168, log(outside[2]));
  EXPECT_FLOAT_EQ(-0.96, log(inside[3]));
  EXPECT_FLOAT_EQ(-0.8261683, log(outside[3]));
  EXPECT_FLOAT_EQ(-1.562512, log(inside[4]));
  EXPECT_FLOAT_EQ(-0.22365622, log(outside[4]));
  EXPECT_FLOAT_EQ(-1.7861683, log(inside[5]));
  EXPECT_FLOAT_EQ(0, log(outside[5]));
}

TEST_F(HGTest,TestAddExpectations) {
  Hypergraph hg;
  CreateHG(&hg);
  SparseVector<double> wts;
  wts.set_value(FD::Convert("f1"), 0.4);
  wts.set_value(FD::Convert("f2"), 0.8);
  hg.Reweight(wts);
  SparseVector<prob_t> feat_exps;
  prob_t z = InsideOutside<prob_t, EdgeProb,
                  SparseVector<prob_t>, EdgeFeaturesAndProbWeightFunction>(hg, &feat_exps);
  EXPECT_FLOAT_EQ(-2.5439765, feat_exps[FD::Convert("f1")] / z);
  EXPECT_FLOAT_EQ(-2.6357865, feat_exps[FD::Convert("f2")] / z);
  cerr << feat_exps << endl;
  cerr << "Z=" << z << endl;
}

TEST_F(HGTest, Small) {
  ReadFile rf("test_data/small.json.gz");
  Hypergraph hg;
  assert(HypergraphIO::ReadFromJSON(rf.stream(), &hg));
  SparseVector<double> wts;
  wts.set_value(FD::Convert("Model_0"), -2.0);
  wts.set_value(FD::Convert("Model_1"), -0.5);
  wts.set_value(FD::Convert("Model_2"), -1.1);
  wts.set_value(FD::Convert("Model_3"), -1.0);
  wts.set_value(FD::Convert("Model_4"), -1.0);
  wts.set_value(FD::Convert("Model_5"), 0.5);
  wts.set_value(FD::Convert("Model_6"), 0.2);
  wts.set_value(FD::Convert("Model_7"), -3.0);
  hg.Reweight(wts);
  vector<WordID> trans;
  prob_t cost = ViterbiESentence(hg, &trans);
  cerr << TD::GetString(trans) << "\n";
  cerr << "cost: " << cost << "\n";
  vector<prob_t> post;
  prob_t c2 = Inside<prob_t, ScaledEdgeProb>(hg, NULL, ScaledEdgeProb(0.6));
  EXPECT_FLOAT_EQ(2.1431036, log(c2));
}

TEST_F(HGTest, JSONTest) {
  ostringstream os;
  JSONParser::WriteEscapedString("\"I don't know\", she said.", &os);
  EXPECT_EQ("\"\\\"I don't know\\\", she said.\"", os.str());
  ostringstream os2;
  JSONParser::WriteEscapedString("yes", &os2);
  EXPECT_EQ("\"yes\"", os2.str());
}

TEST_F(HGTest, TestGenericKBest) {
  Hypergraph hg;
  CreateHG(&hg);
  //CreateHGBalanced(&hg);
  SparseVector<double> wts;
  wts.set_value(FD::Convert("f1"), 0.4);
  wts.set_value(FD::Convert("f2"), 1.0);
  hg.Reweight(wts);
  vector<WordID> trans;
  prob_t cost = ViterbiESentence(hg, &trans);
  cerr << TD::GetString(trans) << "\n";
  cerr << "cost: " << cost << "\n";

  KBest::KBestDerivations<vector<WordID>, ESentenceTraversal> kbest(hg, 1000);
  for (int i = 0; i < 1000; ++i) {
    const KBest::KBestDerivations<vector<WordID>, ESentenceTraversal>::Derivation* d =
      kbest.LazyKthBest(hg.nodes_.size() - 1, i);
    if (!d) break;
    cerr << TD::GetString(d->yield) << " F:" << d->feature_values << endl;
  }
}

TEST_F(HGTest, TestReadWriteHG) {
  Hypergraph hg,hg2;
  CreateHG(&hg);
  hg.edges_.front().j_ = 23;
  hg.edges_.back().prev_i_ = 99;
  ostringstream os;
  HypergraphIO::WriteToJSON(hg, false, &os);
  istringstream is(os.str());
  HypergraphIO::ReadFromJSON(&is, &hg2);
  EXPECT_EQ(hg2.NumberOfPaths(), hg.NumberOfPaths());
  EXPECT_EQ(hg2.edges_.front().j_, 23);
  EXPECT_EQ(hg2.edges_.back().prev_i_, 99);
}

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
