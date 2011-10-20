#include <iostream>
#include <fstream>
#include <vector>
#include "tdict.h"

#include "json_parse.h"
#include "hg_intersect.h"
#include "viterbi.h"
#include "kbest.h"
#include "inside_outside.h"

#include "hg_test.h"


using namespace std;

typedef HGSetup HGTest;

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
  EXPECT_FLOAT_EQ(c2.as_float(), c3.as_float());
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
  EXPECT_FLOAT_EQ(1.7934048, inside.as_float());  // computed by hand
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
  hg.DensityPruneInsideOutside(0.5, false, 2.0);
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
  EXPECT_EQ(inplf,outplf);
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
  EXPECT_FLOAT_EQ(1.7934048, ins.as_float());  // computed by hand
  vector<prob_t> outside;
  Outside<prob_t, EdgeProb>(hg, inside, &outside);
  EXPECT_EQ(3, outside.size());
  EXPECT_FLOAT_EQ(1.7934048, outside[0].as_float());
  EXPECT_FLOAT_EQ(1.3114071, outside[1].as_float());
  EXPECT_FLOAT_EQ(1.0, outside[2].as_float());
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
  EXPECT_FLOAT_EQ(-2.5439765, (feat_exps.value(FD::Convert("f1")) / z).as_float());
  EXPECT_FLOAT_EQ(-2.6357865, (feat_exps.value(FD::Convert("f2")) / z).as_float());
  cerr << feat_exps << endl;
  cerr << "Z=" << z << endl;
}

TEST_F(HGTest, Small) {
  Hypergraph hg;
  CreateSmallHG(&hg);
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
