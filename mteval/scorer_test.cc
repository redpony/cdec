#include <iostream>
#define BOOST_TEST_MODULE ScoreTest
#include <boost/test/unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp>

#include "ns.h"
#include "tdict.h"
#include "scorer.h"
#include "aer_scorer.h"
#include "kernel_string_subseq.h"

using namespace std;

struct Stuff {
 Stuff() {
     refs0.resize(4);
     refs1.resize(4);
     TD::ConvertSentence("export of high-tech products in guangdong in first two months this year reached 3.76 billion us dollars", &refs0[0]);
     TD::ConvertSentence("guangdong's export of new high technology products amounts to us $ 3.76 billion in first two months of this year", &refs0[1]);
     TD::ConvertSentence("guangdong exports us $ 3.76 billion worth of high technology products in the first two months of this year", &refs0[2]);
     TD::ConvertSentence("in the first 2 months this year , the export volume of new hi-tech products in guangdong province reached 3.76 billion us dollars .", &refs0[3]);
     TD::ConvertSentence("xinhua news agency , guangzhou , march 16 ( reporter chen ji ) the latest statistics show that from january through february this year , the export of high-tech products in guangdong province reached 3.76 billion us dollars , up 34.8 \% over the same period last year and accounted for 25.5 \% of the total export in the province .", &refs1[0]);
     TD::ConvertSentence("xinhua news agency , guangzhou , march 16 ( reporter : chen ji ) -- latest statistic indicates that guangdong's export of new high technology products amounts to us $ 3.76 billion , up 34.8 \% over corresponding period and accounts for 25.5 \% of the total exports of the province .", &refs1[1]);
     TD::ConvertSentence("xinhua news agency report of march 16 from guangzhou ( by staff reporter chen ji ) - latest statistics indicate guangdong province exported us $ 3.76 billion worth of high technology products , up 34.8 percent from the same period last year , which account for 25.5 percent of the total exports of the province .", &refs1[2]);
     TD::ConvertSentence("guangdong , march 16 , ( xinhua ) -- ( chen ji reports ) as the newest statistics shows , in january and feberuary this year , the export volume of new hi-tech products in guangdong province reached 3.76 billion us dollars , up 34.8 \% than last year , making up 25.5 \% of the province's total .", &refs1[3]);
     TD::ConvertSentence("one guangdong province will next export us $ 3.76 high-tech product two months first this year 3.76 billion us dollars", &hyp1);
     TD::ConvertSentence("xinhua news agency , guangzhou , 16th of march ( reporter chen ) -- latest statistics suggest that guangdong exports new advanced technology product totals $ 3.76 million , 34.8 percent last corresponding period and accounts for 25.5 percent of the total export province .", &hyp2);
   }

  vector<vector<WordID> > refs0;
  vector<vector<WordID> > refs1;
  vector<WordID> hyp1;
  vector<WordID> hyp2;
};

BOOST_FIXTURE_TEST_SUITE( s, Stuff );

BOOST_AUTO_TEST_CASE(TestCreateFromFiles) {
  std::string path(boost::unit_test::framework::master_test_suite().argc == 2 ? boost::unit_test::framework::master_test_suite().argv[1] : TEST_DATA);
  vector<string> files;
  files.push_back(path + "/re.txt.0");
  files.push_back(path + "/re.txt.1");
  files.push_back(path + "/re.txt.2");
  files.push_back(path + "/re.txt.3");
  DocScorer ds(IBM_BLEU, files);
}

BOOST_AUTO_TEST_CASE(TestBLEUScorer) {
  ScorerP s1 = SentenceScorer::CreateSentenceScorer(IBM_BLEU, refs0);
  ScorerP s2 = SentenceScorer::CreateSentenceScorer(IBM_BLEU, refs1);
  ScoreP b1 = s1->ScoreCandidate(hyp1);
  BOOST_CHECK_CLOSE(0.23185077, b1->ComputeScore(), 1e-4);
  ScoreP b2 = s2->ScoreCandidate(hyp2);
  BOOST_CHECK_CLOSE(0.38101241, b2->ComputeScore(), 1e-4);
  b1->PlusEquals(*b2);
  BOOST_CHECK_CLOSE(0.348854, b1->ComputeScore(), 1e-4);
  BOOST_CHECK(!b1->IsAdditiveIdentity());
  string details;
  b1->ScoreDetails(&details);
  BOOST_CHECK_EQUAL("BLEU = 34.89, 81.5|50.8|29.5|18.6 (brev=0.898)", details);
  cerr << details << endl;
  string enc;
  b1->Encode(&enc);
  ScoreP b3 = SentenceScorer::CreateScoreFromString(IBM_BLEU, enc);
  details.clear();
  cerr << "Encoded BLEU score size: " << enc.size() << endl;
  b3->ScoreDetails(&details);
  cerr << details << endl;
  BOOST_CHECK(!b3->IsAdditiveIdentity());
  BOOST_CHECK_EQUAL("BLEU = 34.89, 81.5|50.8|29.5|18.6 (brev=0.898)", details);
  ScoreP bz = b3->GetZero();
  BOOST_CHECK(bz->IsAdditiveIdentity());
}

BOOST_AUTO_TEST_CASE(TestTERScorer) {
  ScorerP s1 = SentenceScorer::CreateSentenceScorer(TER, refs0);
  ScorerP s2 = SentenceScorer::CreateSentenceScorer(TER, refs1);
  string details;
  ScoreP t1 = s1->ScoreCandidate(hyp1);
  t1->ScoreDetails(&details);
  cerr << "DETAILS: " << details << endl;
  cerr << t1->ComputeScore() << endl;
  ScoreP t2 = s2->ScoreCandidate(hyp2);
  t2->ScoreDetails(&details);
  cerr << "DETAILS: " << details << endl;
  cerr << t2->ComputeScore() << endl;
  t1->PlusEquals(*t2);
  cerr << t1->ComputeScore() << endl;
  t1->ScoreDetails(&details);
  cerr << "DETAILS: " << details << endl;
  BOOST_CHECK_EQUAL("TER = 44.16,   4|  8| 16|  6 (len=77)", details);
  string enc;
  t1->Encode(&enc);
  ScoreP t3 = SentenceScorer::CreateScoreFromString(TER, enc);
  details.clear();
  t3->ScoreDetails(&details);
  BOOST_CHECK_EQUAL("TER = 44.16,   4|  8| 16|  6 (len=77)", details);
  BOOST_CHECK(!t3->IsAdditiveIdentity());
  ScoreP tz = t3->GetZero();
  BOOST_CHECK(tz->IsAdditiveIdentity());
}

BOOST_AUTO_TEST_CASE(TestTERScorerSimple) {
  vector<vector<WordID> > ref(1);
  TD::ConvertSentence("1 2 3 A B", &ref[0]);
  vector<WordID> hyp;
  TD::ConvertSentence("A B 1 2 3", &hyp);
  ScorerP s1 = SentenceScorer::CreateSentenceScorer(TER, ref);
  string details;
  ScoreP t1 = s1->ScoreCandidate(hyp);
  t1->ScoreDetails(&details);
  cerr << "DETAILS: " << details << endl;
}

BOOST_AUTO_TEST_CASE(TestSERScorerSimple) {
  vector<vector<WordID> > ref(1);
  TD::ConvertSentence("A B C D", &ref[0]);
  vector<WordID> hyp1;
  TD::ConvertSentence("A B C", &hyp1);
  vector<WordID> hyp2;
  TD::ConvertSentence("A B C D", &hyp2);
  ScorerP s1 = SentenceScorer::CreateSentenceScorer(SER, ref);
  string details;
  ScoreP t1 = s1->ScoreCandidate(hyp1);
  t1->ScoreDetails(&details);
  cerr << "DETAILS: " << details << endl;
  ScoreP t2 = s1->ScoreCandidate(hyp2);
  t2->ScoreDetails(&details);
  cerr << "DETAILS: " << details << endl;
  t2->PlusEquals(*t1);
  t2->ScoreDetails(&details);
  cerr << "DETAILS: " << details << endl;
}

BOOST_AUTO_TEST_CASE(TestCombiScorer) {
  ScorerP s1 = SentenceScorer::CreateSentenceScorer(BLEU_minus_TER_over_2, refs0);
  string details;
  ScoreP t1 = s1->ScoreCandidate(hyp1);
  t1->ScoreDetails(&details);
  cerr << "DETAILS: " << details << endl;
  cerr << t1->ComputeScore() << endl;
  string enc;
  t1->Encode(&enc);
  ScoreP t2 = SentenceScorer::CreateScoreFromString(BLEU_minus_TER_over_2, enc);
  details.clear();
  t2->ScoreDetails(&details);
  cerr << "DETAILS: " << details << endl;
  ScoreP cz = t2->GetZero();
  BOOST_CHECK(!t2->IsAdditiveIdentity());
  BOOST_CHECK(cz->IsAdditiveIdentity());
  cz->PlusEquals(*t2);
  BOOST_CHECK(!cz->IsAdditiveIdentity());
  string d2;
  cz->ScoreDetails(&d2);
  BOOST_CHECK_EQUAL(d2, details);
}

BOOST_AUTO_TEST_CASE(AERTest) {
  vector<vector<WordID> > refs0(1);
  TD::ConvertSentence("0-0 2-1 1-2 3-3", &refs0[0]);

  vector<WordID> hyp;
  TD::ConvertSentence("0-0 1-1", &hyp);
  AERScorer* as = new AERScorer(refs0);
  ScoreP x = as->ScoreCandidate(hyp);
  string details;
  x->ScoreDetails(&details);
  cerr << details << endl;
  string enc;
  x->Encode(&enc);
  delete as;
  cerr << "ENC size: " << enc.size() << endl;
  ScoreP y = SentenceScorer::CreateScoreFromString(AER, enc);
  string d2;
  y->ScoreDetails(&d2);
  cerr << d2 << endl;
  BOOST_CHECK_EQUAL(d2, details);
}

BOOST_AUTO_TEST_CASE(Kernel) {
  for (int i = 1; i < 10; ++i) {
    const float l = (i / 10.0);
    float f = ssk<4>(refs0[0], hyp1, l) +
              ssk<4>(refs0[1], hyp1, l) +
              ssk<4>(refs0[2], hyp1, l) +
              ssk<4>(refs0[3], hyp1, l);
    float f2= ssk<4>(refs1[0], hyp2, l) +
              ssk<4>(refs1[1], hyp2, l) +
              ssk<4>(refs1[2], hyp2, l) +
              ssk<4>(refs1[3], hyp2, l);
    f /= 4;
    f2 /= 4;
    float f3= ssk<4>(refs0[0], hyp2, l) +
              ssk<4>(refs0[1], hyp2, l) +
              ssk<4>(refs0[2], hyp2, l) +
              ssk<4>(refs0[3], hyp2, l);
    float f4= ssk<4>(refs1[0], hyp1, l) +
              ssk<4>(refs1[1], hyp1, l) +
              ssk<4>(refs1[2], hyp1, l) +
              ssk<4>(refs1[3], hyp1, l);
    f3 += f4;
    f3 /= 8;
    cerr << "LAMBDA=" << l << "\t" << f << " " << f2 << "\tf=" << ((f + f2)/2 - f3) << " (bad=" << f3 << ")" << endl;
  }
}

BOOST_AUTO_TEST_CASE(NewScoreAPI) {
  //EvaluationMetric* metric = EvaluationMetric::Instance("IBM_BLEU");
  //EvaluationMetric* metric = EvaluationMetric::Instance("METEOR");
  EvaluationMetric* metric = EvaluationMetric::Instance("COMB:IBM_BLEU=0.5;TER=-0.5");
  boost::shared_ptr<SegmentEvaluator> e1 = metric->CreateSegmentEvaluator(refs0);
  boost::shared_ptr<SegmentEvaluator> e2 = metric->CreateSegmentEvaluator(refs1);
  SufficientStats stats1;
  e1->Evaluate(hyp1, &stats1);
  SufficientStats stats2;
  e2->Evaluate(hyp2, &stats2);
  stats1 += stats2;
  string ss;
  stats1.Encode(&ss);
  cerr << "SS: " << ss << endl;
  cerr << metric->ComputeScore(stats1) << endl;
  //SufficientStats statse("IBM_BLEU 53 32 18 11 65 63 61 59 65 72");
  //cerr << metric->ComputeScore(statse) << endl;
}

BOOST_AUTO_TEST_SUITE_END()
