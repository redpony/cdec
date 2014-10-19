#include "trule.h"

#define BOOST_TEST_MODULE TRuleTest
#include <boost/test/unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp>
#include <iostream>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <sstream>
#include "tdict.h"

using namespace std;

BOOST_AUTO_TEST_CASE(TestFSubstitute) {
  TRule r1("[X] ||| ob [X,1] [X,2] sah . ||| whether [X,1] saw [X,2] . ||| 0.99");
  TRule r2("[X] ||| ich ||| i ||| 1.0");
  TRule r3("[X] ||| ihn ||| him ||| 1.0");
  vector<const vector<WordID>*> ants;
  vector<WordID> res2;
  r2.FSubstitute(ants, &res2);
  assert(TD::GetString(res2) == "ich");
  vector<WordID> res3;
  r3.FSubstitute(ants, &res3);
  assert(TD::GetString(res3) == "ihn");
  ants.push_back(&res2);
  ants.push_back(&res3);
  vector<WordID> res;
  r1.FSubstitute(ants, &res);
  cerr << TD::GetString(res) << endl;
  assert(TD::GetString(res) == "ob ich ihn sah .");
}

BOOST_AUTO_TEST_CASE(TestPhrasetableRule) {
  TRulePtr t(TRule::CreateRulePhrasetable("gato ||| cat ||| PhraseModel_0=-23.2;Foo=1;Bar=12"));
  cerr << t->AsString() << endl;
  assert(t->scores_.size() == 3);
};


BOOST_AUTO_TEST_CASE(TestMonoRule) {
  TRulePtr m(TRule::CreateRuleMonolingual("[LHS] ||| term1 [NT] term2 [NT2] [NT3]"));
  assert(m->Arity() == 3);
  cerr << m->AsString() << endl;
  TRulePtr m2(TRule::CreateRuleMonolingual("[LHS] ||| term1 [NT] term2 [NT2] [NT3] ||| Feature1=0.23"));
  assert(m2->Arity() == 3);
  cerr << m2->AsString() << endl;
  BOOST_CHECK_CLOSE(m2->scores_.value(FD::Convert("Feature1")), 0.23, 1e-6);
}

BOOST_AUTO_TEST_CASE(TestRuleR) {
  TRule t6;
  t6.ReadFromString("[X] ||| den [X,1] sah [X,2] . ||| [X,2] saw the [X,1] . ||| 0.12321 0.23232 0.121");
  cerr << "TEXT: " << t6.AsString() << endl;
  BOOST_CHECK_EQUAL(t6.Arity(), 2);
  BOOST_CHECK_EQUAL(t6.e_[0], -1);
  BOOST_CHECK_EQUAL(t6.e_[3], 0);
}

BOOST_AUTO_TEST_CASE(TestReadWriteHG_Boost) {
  string str;
  string t7str;
  {
    TRule t7;
    t7.ReadFromString("[X] ||| den [X,1] sah [X,2] . ||| [2] saw the [1] . ||| Feature1=0.12321 Foo=0.23232 Bar=0.121");
    cerr << t7.AsString() << endl;
    ostringstream os;
    TRulePtr tp1(new TRule("[X] ||| a b c ||| x z y ||| A=1 B=2"));
    TRulePtr tp2 = tp1;
    boost::archive::text_oarchive oa(os);
    oa << t7;
    oa << tp1;
    oa << tp2;
    str = os.str();
    t7str = t7.AsString();
  }
  {
    istringstream is(str);
    boost::archive::text_iarchive ia(is);
    TRule t8;
    ia >> t8;
    TRulePtr tp3, tp4;
    ia >> tp3;
    ia >> tp4;
    cerr << t8.AsString() << endl;
    BOOST_CHECK_EQUAL(t7str, t8.AsString());
    cerr << tp3->AsString() << endl;
    cerr << tp4->AsString() << endl;
  }
}

