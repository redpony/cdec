#include <cmath>
#include <iostream>
#include <fstream>

#include <boost/shared_ptr.hpp>
#include <gtest/gtest.h>

#include "fdict.h"
#include "hg.h"
#include "kbest.h"
#include "hg_io.h"
#include "filelib.h"
#include "inside_outside.h"
#include "viterbi.h"
#include "viterbi_envelope.h"
#include "line_optimizer.h"
#include "scorer.h"

using namespace std;
using boost::shared_ptr;

class OptTest : public testing::Test {
 protected:
   virtual void SetUp() { }
   virtual void TearDown() { }
};

const char* ref11 = "australia reopens embassy in manila";
const char* ref12 = "( afp , manila , january 2 ) australia reopened its embassy in the philippines today , which was shut down about seven weeks ago due to what was described as a specific threat of a terrorist attack .";
const char* ref21 = "australia reopened manila embassy";
const char* ref22 = "( agence france-presse , manila , 2nd ) - australia reopened its embassy in the philippines today . the embassy was closed seven weeks ago after what was described as a specific threat of a terrorist attack .";
const char* ref31 = "australia to reopen embassy in manila";
const char* ref32 = "( afp report from manila , january 2 ) australia reopened its embassy in the philippines today . seven weeks ago , the embassy was shut down due to so - called confirmed terrorist attack threats .";
const char* ref41 = "australia to re - open its embassy to manila";
const char* ref42 = "( afp , manila , thursday ) australia reopens its embassy to manila , which was closed for the so - called \" clear \" threat of terrorist attack 7 weeks ago .";

TEST_F(OptTest, TestCheckNaN) {
  double x = 0;
  double y = 0;
  double z = x / y;
  EXPECT_EQ(true, isnan(z));
}

TEST_F(OptTest,TestViterbiEnvelope) {
  shared_ptr<Segment> a1(new Segment(-1, 0));
  shared_ptr<Segment> b1(new Segment(1, 0));
  shared_ptr<Segment> a2(new Segment(-1, 1));
  shared_ptr<Segment> b2(new Segment(1, -1));
  vector<shared_ptr<Segment> > sa; sa.push_back(a1); sa.push_back(b1);
  vector<shared_ptr<Segment> > sb; sb.push_back(a2); sb.push_back(b2);
  ViterbiEnvelope a(sa);
  cerr << a << endl;
  ViterbiEnvelope b(sb);
  ViterbiEnvelope c = a;
  c *= b;
  cerr << a << " (*) " << b << " = " << c << endl;
  EXPECT_EQ(3, c.size());
}

TEST_F(OptTest,TestViterbiEnvelopeInside) {
  const string json = "{\"rules\":[1,\"[X] ||| a\",2,\"[X] ||| A [1]\",3,\"[X] ||| c\",4,\"[X] ||| C [1]\",5,\"[X] ||| [1] B [2]\",6,\"[X] ||| [1] b [2]\",7,\"[X] ||| X [1]\",8,\"[X] ||| Z [1]\"],\"features\":[\"f1\",\"f2\",\"Feature_1\",\"Feature_0\",\"Model_0\",\"Model_1\",\"Model_2\",\"Model_3\",\"Model_4\",\"Model_5\",\"Model_6\",\"Model_7\"],\"edges\":[{\"tail\":[],\"feats\":[],\"rule\":1}],\"node\":{\"in_edges\":[0]},\"edges\":[{\"tail\":[0],\"feats\":[0,-0.8,1,-0.1],\"rule\":2}],\"node\":{\"in_edges\":[1]},\"edges\":[{\"tail\":[],\"feats\":[1,-1],\"rule\":3}],\"node\":{\"in_edges\":[2]},\"edges\":[{\"tail\":[2],\"feats\":[0,-0.2,1,-0.1],\"rule\":4}],\"node\":{\"in_edges\":[3]},\"edges\":[{\"tail\":[1,3],\"feats\":[0,-1.2,1,-0.2],\"rule\":5},{\"tail\":[1,3],\"feats\":[0,-0.5,1,-1.3],\"rule\":6}],\"node\":{\"in_edges\":[4,5]},\"edges\":[{\"tail\":[4],\"feats\":[0,-0.5,1,-0.8],\"rule\":7},{\"tail\":[4],\"feats\":[0,-0.7,1,-0.9],\"rule\":8}],\"node\":{\"in_edges\":[6,7]}}";
  Hypergraph hg;
  istringstream instr(json);
  HypergraphIO::ReadFromJSON(&instr, &hg);
  SparseVector<double> wts;
  wts.set_value(FD::Convert("f1"), 0.4);
  wts.set_value(FD::Convert("f2"), 1.0);
  hg.Reweight(wts);
  vector<pair<vector<WordID>, prob_t> > list;
  std::vector<SparseVector<double> > features;
  KBest::KBestDerivations<vector<WordID>, ESentenceTraversal> kbest(hg, 10);
  for (int i = 0; i < 10; ++i) {
    const KBest::KBestDerivations<vector<WordID>, ESentenceTraversal>::Derivation* d =
      kbest.LazyKthBest(hg.nodes_.size() - 1, i);
    if (!d) break;
    cerr << log(d->score) << " ||| " << TD::GetString(d->yield) << " ||| " << d->feature_values << endl;
  }
  SparseVector<double> dir; dir.set_value(FD::Convert("f1"), 1.0);
  ViterbiEnvelopeWeightFunction wf(wts, dir);
  ViterbiEnvelope env = Inside<ViterbiEnvelope, ViterbiEnvelopeWeightFunction>(hg, NULL, wf);
  cerr << env << endl;
  const vector<boost::shared_ptr<Segment> >& segs = env.GetSortedSegs();
  dir *= segs[1]->x;
  wts += dir;
  hg.Reweight(wts);
  KBest::KBestDerivations<vector<WordID>, ESentenceTraversal> kbest2(hg, 10);
  for (int i = 0; i < 10; ++i) {
    const KBest::KBestDerivations<vector<WordID>, ESentenceTraversal>::Derivation* d =
      kbest2.LazyKthBest(hg.nodes_.size() - 1, i);
    if (!d) break;
    cerr << log(d->score) << " ||| " << TD::GetString(d->yield) << " ||| " << d->feature_values << endl;
  }
  for (int i = 0; i < segs.size(); ++i) {
    cerr << "seg=" << i << endl;
    vector<WordID> trans;
    segs[i]->ConstructTranslation(&trans);
    cerr << TD::GetString(trans) << endl;
  }
}

TEST_F(OptTest, TestS1) { 
  int fPhraseModel_0 = FD::Convert("PhraseModel_0");
  int fPhraseModel_1 = FD::Convert("PhraseModel_1");
  int fPhraseModel_2 = FD::Convert("PhraseModel_2");
  int fLanguageModel = FD::Convert("LanguageModel");
  int fWordPenalty = FD::Convert("WordPenalty");
  int fPassThrough = FD::Convert("PassThrough");
  SparseVector<double> wts;
  wts.set_value(fWordPenalty, 4.25);
  wts.set_value(fLanguageModel, -1.1165);
  wts.set_value(fPhraseModel_0, -0.96);
  wts.set_value(fPhraseModel_1, -0.65);
  wts.set_value(fPhraseModel_2, -0.77);
  wts.set_value(fPassThrough, -10.0);

  vector<int> to_optimize;
  to_optimize.push_back(fWordPenalty);
  to_optimize.push_back(fLanguageModel);
  to_optimize.push_back(fPhraseModel_0);
  to_optimize.push_back(fPhraseModel_1);
  to_optimize.push_back(fPhraseModel_2);

  Hypergraph hg;
  ReadFile rf("./test_data/0.json.gz");
  HypergraphIO::ReadFromJSON(rf.stream(), &hg);
  hg.Reweight(wts);

  Hypergraph hg2;
  ReadFile rf2("./test_data/1.json.gz");
  HypergraphIO::ReadFromJSON(rf2.stream(), &hg2);
  hg2.Reweight(wts);

  vector<vector<WordID> > refs1(4);
  TD::ConvertSentence(ref11, &refs1[0]);
  TD::ConvertSentence(ref21, &refs1[1]);
  TD::ConvertSentence(ref31, &refs1[2]);
  TD::ConvertSentence(ref41, &refs1[3]);
  vector<vector<WordID> > refs2(4);
  TD::ConvertSentence(ref12, &refs2[0]);
  TD::ConvertSentence(ref22, &refs2[1]);
  TD::ConvertSentence(ref32, &refs2[2]);
  TD::ConvertSentence(ref42, &refs2[3]);
  ScoreType type = ScoreTypeFromString("ibm_bleu");
  SentenceScorer* scorer1 = SentenceScorer::CreateSentenceScorer(type, refs1);
  SentenceScorer* scorer2 = SentenceScorer::CreateSentenceScorer(type, refs2);
  vector<ViterbiEnvelope> envs(2);
  
  RandomNumberGenerator<boost::mt19937> rng;

  vector<SparseVector<double> > axes;
  LineOptimizer::CreateOptimizationDirections(
     to_optimize,
     10,
     &rng,
     &axes);
  assert(axes.size() == 10 + to_optimize.size());
  for (int i = 0; i < axes.size(); ++i)
    cerr << axes[i] << endl;
  const SparseVector<double>& axis = axes[0];

  cerr << "Computing Viterbi envelope using inside algorithm...\n";
  cerr << "axis: " << axis << endl;
  clock_t t_start=clock();
  ViterbiEnvelopeWeightFunction wf(wts, axis);
  envs[0] = Inside<ViterbiEnvelope, ViterbiEnvelopeWeightFunction>(hg, NULL, wf);
  envs[1] = Inside<ViterbiEnvelope, ViterbiEnvelopeWeightFunction>(hg2, NULL, wf);

  vector<ErrorSurface> es(2);
  scorer1->ComputeErrorSurface(envs[0], &es[0], IBM_BLEU, hg);
  scorer2->ComputeErrorSurface(envs[1], &es[1], IBM_BLEU, hg2);
  cerr << envs[0].size() << " " << envs[1].size() << endl;
  cerr << es[0].size() << " " << es[1].size() << endl;
  envs.clear();
  clock_t t_env=clock();
  float score;
  double m = LineOptimizer::LineOptimize(es, LineOptimizer::MAXIMIZE_SCORE, &score);
  clock_t t_opt=clock();
  cerr << "line optimizer returned: " << m << " (SCORE=" << score << ")\n";
  EXPECT_FLOAT_EQ(0.48719698, score);
  SparseVector<double> res = axis;
  res *= m;
  res += wts;
  cerr << "res: " << res << endl;
  cerr << "ENVELOPE PROCESSING=" << (static_cast<double>(t_env - t_start) / 1000.0) << endl;
  cerr << "  LINE OPTIMIZATION=" << (static_cast<double>(t_opt - t_env) / 1000.0) << endl;
  hg.Reweight(res);
  hg2.Reweight(res);
  vector<WordID> t1,t2;
  ViterbiESentence(hg, &t1);
  ViterbiESentence(hg2, &t2);
  cerr << TD::GetString(t1) << endl;
  cerr << TD::GetString(t2) << endl;
  delete scorer1;
  delete scorer2;
}

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

