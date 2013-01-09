#define BOOST_TEST_MODULE hg_test
#include <boost/test/unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp>
#include <iostream>
#include "tdict.h"

#include "json_parse.h"
#include "hg_intersect.h"
#include "hg_union.h"
#include "viterbi.h"
#include "kbest.h"
#include "inside_outside.h"

#include "hg_test.h"

using namespace std;

BOOST_FIXTURE_TEST_SUITE( s, HGSetup );

BOOST_AUTO_TEST_CASE(Controlled) {
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
  BOOST_CHECK_CLOSE(-80.839996, log(prob), 1e-4);
  BOOST_CHECK_EQUAL("X <s>", TD::GetString(trans));
  vector<prob_t> post;
  hg.PrintGraphviz();
  prob_t c2 = Inside<prob_t, ScaledEdgeProb>(hg, NULL, ScaledEdgeProb(0.6));
  BOOST_CHECK_CLOSE(-47.8577, log(c2), 1e-4);
}

BOOST_AUTO_TEST_CASE(Union) {
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
  HG::Union(hg2, &hg1);
  hg1.Reweight(wts);
  c3 = ViterbiESentence(hg1, &t3);
  int l3 = ViterbiPathLength(hg1);
  cerr << c3 << "\t" << TD::GetString(t3) << endl;
  BOOST_CHECK_CLOSE(c2.as_float(), c3.as_float(), 1e-4);
  BOOST_CHECK_EQUAL(TD::GetString(t2), TD::GetString(t3));
  BOOST_CHECK_EQUAL(l2, l3);

  wts.set_value(FD::Convert("f2"), -1);
  hg1.Reweight(wts);
  c4 = ViterbiESentence(hg1, &t4);
  cerr << c4 << "\t" << TD::GetString(t4) << endl;
  BOOST_CHECK_EQUAL("Z <s>", TD::GetString(t4));
  BOOST_CHECK_CLOSE(98.82, log(c4), 1e-4);

  vector<pair<vector<WordID>, prob_t> > list;
  KBest::KBestDerivations<vector<WordID>, ESentenceTraversal> kbest(hg1, 10);
  for (int i = 0; i < 10; ++i) {
    const KBest::KBestDerivations<vector<WordID>, ESentenceTraversal>::Derivation* d =
      kbest.LazyKthBest(hg1.nodes_.size() - 1, i);
    if (!d) break;
    list.push_back(make_pair(d->yield, d->score));
  }
  BOOST_CHECK(list[0].first == t4);
  BOOST_CHECK_CLOSE(log(list[0].second), log(c4), 1e-4);
  BOOST_CHECK_EQUAL(list.size(), 6);
  BOOST_CHECK_CLOSE(log(list.back().second / list.front().second), -97.7, 1e-4);
}

BOOST_AUTO_TEST_CASE(ControlledKBest) {
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
  BOOST_CHECK_EQUAL(4, best);
}


BOOST_AUTO_TEST_CASE(InsideScore) {
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
  BOOST_CHECK_CLOSE(1.7934048, inside.as_float(), 1e-4);  // computed by hand
  vector<prob_t> post;
  inside = hg.ComputeBestPathThroughEdges(&post);
  BOOST_CHECK_CLOSE(-0.3, log(inside), 1e-4);  // computed by hand
  BOOST_CHECK_EQUAL(post.size(), 5);
  for (int i = 0; i < 5; ++i) {
    cerr << "edge post: " << log(post[i]) << '\t' << hg.edges_[i].rule_->AsString() << endl;
  }
}


BOOST_AUTO_TEST_CASE(PruneInsideOutside) {
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
#if 0
  hg.DensityPruneInsideOutside(0.5, false, 2.0);
  hg.BeamPruneInsideOutside(0.5, false, 0.5);
  cost = ViterbiESentence(hg, &trans);
  cerr << "Ncst: " << cost << endl;
  cerr << TD::GetString(trans) << "\n";
  hg.PrintGraphviz();
#endif
  cerr << "FIX PLEASE\n";
}

BOOST_AUTO_TEST_CASE(TestPruneEdges) {
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

BOOST_AUTO_TEST_CASE(TestIntersect) {
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
  BOOST_CHECK_EQUAL(4, best);

  Lattice target(2);
  target[0].push_back(LatticeArc(TD::Convert("a"), 0.0, 1));
  target[1].push_back(LatticeArc(TD::Convert("b"), 0.0, 1));
  HG::Intersect(target, &hg);
  hg.PrintGraphviz();
}

BOOST_AUTO_TEST_CASE(TestPrune2) {
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

BOOST_AUTO_TEST_CASE(Sample) {
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

BOOST_AUTO_TEST_CASE(PLF) {
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
  BOOST_CHECK_EQUAL(inplf,outplf);
}

BOOST_AUTO_TEST_CASE(PushWeightsToGoal) {
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

BOOST_AUTO_TEST_CASE(TestSpecialKBest) {
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

BOOST_AUTO_TEST_CASE(TestGenericViterbi) {
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
  BOOST_CHECK_CLOSE(-80.839996, log(prob), 1e-4);
  BOOST_CHECK_EQUAL("X <s>", TD::GetString(trans));
}

BOOST_AUTO_TEST_CASE(TestGenericInside) {
  Hypergraph hg;
  CreateTinyLatticeHG(&hg);
  SparseVector<double> wts;
  wts.set_value(FD::Convert("f1"), 1.0);
  hg.Reweight(wts);
  vector<prob_t> inside;
  prob_t ins = Inside<prob_t, EdgeProb>(hg, &inside);
  BOOST_CHECK_CLOSE(1.7934048, ins.as_float(), 1e-4);  // computed by hand
  vector<prob_t> outside;
  Outside<prob_t, EdgeProb>(hg, inside, &outside);
  BOOST_CHECK_EQUAL(3, outside.size());
  BOOST_CHECK_CLOSE(1.7934048, outside[0].as_float(), 1e-4);
  BOOST_CHECK_CLOSE(1.3114071, outside[1].as_float(), 1e-4);
  BOOST_CHECK_CLOSE(1.0, outside[2].as_float(), 1e-4);
}

BOOST_AUTO_TEST_CASE(TestGenericInside2) {
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
  BOOST_CHECK_CLOSE(0, log(inside[0]), 1e-4);
  BOOST_CHECK_CLOSE(-1.7861683, log(outside[0]), 1e-4);
  BOOST_CHECK_CLOSE(-0.4, log(inside[1]), 1e-4);
  BOOST_CHECK_CLOSE(-1.3861683, log(outside[1]), 1e-4);
  BOOST_CHECK_CLOSE(-0.8, log(inside[2]), 1e-4);
  BOOST_CHECK_CLOSE(-0.986168, log(outside[2]), 1e-4);
  BOOST_CHECK_CLOSE(-0.96, log(inside[3]), 1e-4);
  BOOST_CHECK_CLOSE(-0.8261683, log(outside[3]), 1e-4);
  BOOST_CHECK_CLOSE(-1.562512, log(inside[4]), 1e-4);
  BOOST_CHECK_CLOSE(-0.22365622, log(outside[4]), 1e-4);
  BOOST_CHECK_CLOSE(-1.7861683, log(inside[5]), 1e-4);
  BOOST_CHECK_CLOSE(0, log(outside[5]), 1e-4);
}

BOOST_AUTO_TEST_CASE(TestAddExpectations) {
  Hypergraph hg;
  CreateHG(&hg);
  SparseVector<double> wts;
  wts.set_value(FD::Convert("f1"), 0.4);
  wts.set_value(FD::Convert("f2"), 0.8);
  hg.Reweight(wts);
  SparseVector<prob_t> feat_exps;
  prob_t z = InsideOutside<prob_t, EdgeProb,
                  SparseVector<prob_t>, EdgeFeaturesAndProbWeightFunction>(hg, &feat_exps);
  BOOST_CHECK_CLOSE(-2.5439765, (feat_exps.value(FD::Convert("f1")) / z).as_float(), 1e-4);
  BOOST_CHECK_CLOSE(-2.6357865, (feat_exps.value(FD::Convert("f2")) / z).as_float(), 1e-4);
  cerr << feat_exps << endl;
  cerr << "Z=" << z << endl;
}

BOOST_AUTO_TEST_CASE(Small) {
  Hypergraph hg;
  std::string path(boost::unit_test::framework::master_test_suite().argc == 2 ? boost::unit_test::framework::master_test_suite().argv[1] : TEST_DATA);
  CreateSmallHG(&hg, path);
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
  BOOST_CHECK_CLOSE(2.1431036, log(c2), 1e-4);
}

BOOST_AUTO_TEST_CASE(JSONTest) {
  ostringstream os;
  JSONParser::WriteEscapedString("\"I don't know\", she said.", &os);
  BOOST_CHECK_EQUAL("\"\\\"I don't know\\\", she said.\"", os.str());
  ostringstream os2;
  JSONParser::WriteEscapedString("yes", &os2);
  BOOST_CHECK_EQUAL("\"yes\"", os2.str());
}

BOOST_AUTO_TEST_CASE(TestGenericKBest) {
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

BOOST_AUTO_TEST_CASE(TestReadWriteHG) {
  Hypergraph hg,hg2;
  CreateHG(&hg);
  hg.edges_.front().j_ = 23;
  hg.edges_.back().prev_i_ = 99;
  ostringstream os;
  HypergraphIO::WriteToJSON(hg, false, &os);
  istringstream is(os.str());
  HypergraphIO::ReadFromJSON(&is, &hg2);
  BOOST_CHECK_EQUAL(hg2.NumberOfPaths(), hg.NumberOfPaths());
  BOOST_CHECK_EQUAL(hg2.edges_.front().j_, 23);
  BOOST_CHECK_EQUAL(hg2.edges_.back().prev_i_, 99);
}

BOOST_AUTO_TEST_SUITE_END()
