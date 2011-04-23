#include "trule.h"

#include <gtest/gtest.h>
#include <cassert>
#include <iostream>
#include "tdict.h"

using namespace std;

class TRuleTest : public testing::Test {
 protected:
  virtual void SetUp() { }
  virtual void TearDown() { }
};

TEST_F(TRuleTest,TestFSubstitute) {
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

TEST_F(TRuleTest,TestPhrasetableRule) {
  TRulePtr t(TRule::CreateRulePhrasetable("gato ||| cat ||| PhraseModel_0=-23.2;Foo=1;Bar=12"));
  cerr << t->AsString() << endl;
  assert(t->scores_.size() == 3);
};


TEST_F(TRuleTest,TestMonoRule) {
  TRulePtr m(TRule::CreateRuleMonolingual("[LHS] ||| term1 [NT] term2 [NT2] [NT3]"));
  assert(m->Arity() == 3);
  cerr << m->AsString() << endl;
  TRulePtr m2(TRule::CreateRuleMonolingual("[LHS] ||| term1 [NT] term2 [NT2] [NT3] ||| Feature1=0.23"));
  assert(m2->Arity() == 3);
  cerr << m2->AsString() << endl;
  EXPECT_FLOAT_EQ(m2->scores_.value(FD::Convert("Feature1")), 0.23);
}

TEST_F(TRuleTest,TestRuleR) {
  TRule t6;
  t6.ReadFromString("[X] ||| den [X,1] sah [X,2] . ||| [X,2] saw the [X,1] . ||| 0.12321 0.23232 0.121");
  cerr << "TEXT: " << t6.AsString() << endl;
  EXPECT_EQ(t6.Arity(), 2);
  EXPECT_EQ(t6.e_[0], -1);
  EXPECT_EQ(t6.e_[3], 0);
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

