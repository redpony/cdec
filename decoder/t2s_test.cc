#include "tree_fragment.h"

#define BOOST_TEST_MODULE T2STest
#include <boost/test/unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp>
#include <iostream>
#include "tdict.h"

using namespace std;

BOOST_AUTO_TEST_CASE(TestTreeFragments) {
  cdec::TreeFragment tree("(S (NP (DT the) (NN boy)) (VP (V saw) (NP (DT a) (NN cat))))");
  cdec::TreeFragment tree2("(S (NP (DT a) (NN cat)) (VP (V ate) (NP (DT the) (NN cake pie))))");
  vector<unsigned> a, b;
  vector<WordID> aw, bw;
  cerr << "TREE1: " << tree << endl;
  cerr << "TREE2: " << tree2 << endl;
  for (auto& sym : tree) {
    if (cdec::IsLHS(sym)) cerr << "(";
    cerr << TD::Convert(sym & cdec::ALL_MASK) << endl;
    if (cdec::IsTerminal(sym)) aw.push_back(sym); else a.push_back(sym);
  }
  for (auto& sym : tree2)
    if (cdec::IsTerminal(sym)) bw.push_back(sym); else b.push_back(sym);
  BOOST_CHECK_EQUAL(a.size(), b.size());
  BOOST_CHECK_EQUAL(aw.size() + 1, bw.size());
  BOOST_CHECK_EQUAL(aw.size(), 5);
  BOOST_CHECK_EQUAL(TD::GetString(aw), "the boy saw a cat");
  BOOST_CHECK_EQUAL(TD::GetString(bw), "a cat ate the cake pie");
  if (a != b) {
    BOOST_CHECK_EQUAL(1,2);
  }

  string nts;
  for (cdec::TreeFragment::iterator it = tree.begin(); it != tree.end(); ++it) {
    if (cdec::IsNT(*it)) {
      if (cdec::IsRHS(*it)) it.truncate();
      if (nts.size()) nts += " ";
      if (cdec::IsLHS(*it)) nts += "(";
      nts += TD::Convert(*it & cdec::ALL_MASK);
      if (cdec::IsFrontier(*it)) nts += "*";
    }
  }
  cerr << "Truncated: " << nts << endl;
  BOOST_CHECK_EQUAL(nts, "(S NP* VP*");

  nts.clear();
  int ntc = 0;
  for (auto it = tree.bfs_begin(); it != tree.bfs_end(); ++it) {
    if (cdec::IsNT(*it)) {
      if (cdec::IsRHS(*it)) {
        ++ntc;
        if (ntc > 1) it.truncate();
      }
      if (nts.size()) nts += " ";
      if (cdec::IsLHS(*it)) nts += "(";
      nts += TD::Convert(*it & cdec::ALL_MASK);
      if (cdec::IsFrontier(*it)) nts += "*";
    }
  }
  BOOST_CHECK_EQUAL(nts, "(S NP VP* (NP DT* NN*");
}

BOOST_AUTO_TEST_CASE(TestSharing) {
  cdec::TreeFragment rule1("(S [NP] [VP])", true);
  cdec::TreeFragment rule2("(S [NP] (VP [V] [NP]))", true);
  string r1,r2;
  for (auto sym : rule1) {
    if (r1.size()) r1 += " ";
    if (cdec::IsLHS(sym)) r1 += "(";
    r1 += TD::Convert(sym & cdec::ALL_MASK);
    if (cdec::IsFrontier(sym)) r1 += "*";
  }
  for (auto sym : rule2) {
    if (r2.size()) r2 += " ";
    if (cdec::IsLHS(sym)) r2 += "(";
    r2 += TD::Convert(sym & cdec::ALL_MASK);
    if (cdec::IsFrontier(sym)) r2 += "*";
  }
  cerr << rule1 << endl;
  cerr << r1 << endl;
  cerr << rule2 << endl;
  cerr << r2 << endl;
  BOOST_CHECK_EQUAL(r1, "(S NP* VP*");
  BOOST_CHECK_EQUAL(r2, "(S NP* VP (VP V* NP*");
}

BOOST_AUTO_TEST_CASE(TestEndInvariants) {
  cdec::TreeFragment tree("(S (NP (DT the) (NN boy)) (VP (V saw) (NP (DT a) (NN cat))))");
  BOOST_CHECK(tree.end().at_end());
  BOOST_CHECK(!tree.begin().at_end());
}

BOOST_AUTO_TEST_CASE(TestBegins) {
  cdec::TreeFragment tree("(S (NP (DT the) (NN boy)) (VP (V saw) (NP (DT a) (NN cat))))");
  for (auto it = tree.begin(1); it != tree.end(); ++it) {
    cerr << TD::Convert(*it & cdec::ALL_MASK) << endl;
  }
}

BOOST_AUTO_TEST_CASE(TestRemainder) {
  cdec::TreeFragment tree("(S (A a) (B b))");
  auto it = tree.begin();
  ++it;
  BOOST_CHECK(cdec::IsRHS(*it));
  cerr << tree << endl;
  auto itr = it.remainder();
  while(itr != tree.end()) {
    cerr << TD::Convert(*itr & cdec::ALL_MASK) << endl;
    ++itr;
  }
}


