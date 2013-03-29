#define BOOST_TEST_MODULE LineOptimizerTest
#include <boost/test/unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp>

#include <cmath>
#include <iostream>
#include <fstream>

#include <boost/shared_ptr.hpp>

#include "ns.h"
#include "ns_docscorer.h"
#include "ces.h"
#include "fdict.h"
#include "hg.h"
#include "kbest.h"
#include "hg_io.h"
#include "filelib.h"
#include "inside_outside.h"
#include "viterbi.h"
#include "mert_geometry.h"
#include "line_optimizer.h"

using namespace std;

const char* ref11 = "australia reopens embassy in manila";
const char* ref12 = "( afp , manila , january 2 ) australia reopened its embassy in the philippines today , which was shut down about seven weeks ago due to what was described as a specific threat of a terrorist attack .";
const char* ref21 = "australia reopened manila embassy";
const char* ref22 = "( agence france-presse , manila , 2nd ) - australia reopened its embassy in the philippines today . the embassy was closed seven weeks ago after what was described as a specific threat of a terrorist attack .";
const char* ref31 = "australia to reopen embassy in manila";
const char* ref32 = "( afp report from manila , january 2 ) australia reopened its embassy in the philippines today . seven weeks ago , the embassy was shut down due to so - called confirmed terrorist attack threats .";
const char* ref41 = "australia to re - open its embassy to manila";
const char* ref42 = "( afp , manila , thursday ) australia reopens its embassy to manila , which was closed for the so - called \" clear \" threat of terrorist attack 7 weeks ago .";

BOOST_AUTO_TEST_CASE( TestCheckNaN) {
  double x = 0;
  double y = 0;
  double z = x / y;
  BOOST_CHECK_EQUAL(true, std::isnan(z));
}

BOOST_AUTO_TEST_CASE(TestConvexHull) {
  boost::shared_ptr<MERTPoint> a1(new MERTPoint(-1, 0));
  boost::shared_ptr<MERTPoint> b1(new MERTPoint(1, 0));
  boost::shared_ptr<MERTPoint> a2(new MERTPoint(-1, 1));
  boost::shared_ptr<MERTPoint> b2(new MERTPoint(1, -1));
  vector<boost::shared_ptr<MERTPoint> > sa; sa.push_back(a1); sa.push_back(b1);
  vector<boost::shared_ptr<MERTPoint> > sb; sb.push_back(a2); sb.push_back(b2);
  ConvexHull a(sa);
  cerr << a << endl;
  ConvexHull b(sb);
  ConvexHull c = a;
  c *= b;
  cerr << a << " (*) " << b << " = " << c << endl;
  BOOST_CHECK_EQUAL(3, c.size());
}

BOOST_AUTO_TEST_CASE(TestConvexHullInside) {
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
  ConvexHullWeightFunction wf(wts, dir);
  ConvexHull env = Inside<ConvexHull, ConvexHullWeightFunction>(hg, NULL, wf);
  cerr << env << endl;
  const vector<boost::shared_ptr<MERTPoint> >& segs = env.GetSortedSegs();
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
  for (unsigned i = 0; i < segs.size(); ++i) {
    cerr << "seg=" << i << endl;
    vector<WordID> trans;
    segs[i]->ConstructTranslation(&trans);
    cerr << TD::GetString(trans) << endl;
  }
}

BOOST_AUTO_TEST_CASE( TestS1) {
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

  std::string path(boost::unit_test::framework::master_test_suite().argc == 2 ? boost::unit_test::framework::master_test_suite().argv[1] : TEST_DATA);

  Hypergraph hg;
  ReadFile rf(path + "/0.json.gz");
  HypergraphIO::ReadFromJSON(rf.stream(), &hg);
  hg.Reweight(wts);

  Hypergraph hg2;
  ReadFile rf2(path + "/1.json.gz");
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
  vector<ConvexHull> envs(2);

  RandomNumberGenerator<boost::mt19937> rng;

  vector<SparseVector<double> > axes; // directions to search
  LineOptimizer::CreateOptimizationDirections(
     to_optimize,
     10,
     &rng,
     &axes);
  assert(axes.size() == 10 + to_optimize.size());
  for (unsigned i = 0; i < axes.size(); ++i)
    cerr << axes[i] << endl;
  const SparseVector<double>& axis = axes[0];

  cerr << "Computing Viterbi envelope using inside algorithm...\n";
  cerr << "axis: " << axis << endl;
  clock_t t_start=clock();
  ConvexHullWeightFunction wf(wts, axis);  // wts = starting point, axis = search direction
  envs[0] = Inside<ConvexHull, ConvexHullWeightFunction>(hg, NULL, wf);
  envs[1] = Inside<ConvexHull, ConvexHullWeightFunction>(hg2, NULL, wf);

  vector<ErrorSurface> es(2);
  EvaluationMetric* metric = EvaluationMetric::Instance("IBM_BLEU");
  boost::shared_ptr<SegmentEvaluator> scorer1 = metric->CreateSegmentEvaluator(refs1);
  boost::shared_ptr<SegmentEvaluator> scorer2 = metric->CreateSegmentEvaluator(refs2);
  ComputeErrorSurface(*scorer1, envs[0], &es[0], metric, hg);
  ComputeErrorSurface(*scorer2, envs[1], &es[1], metric, hg2);
  cerr << envs[0].size() << " " << envs[1].size() << endl;
  cerr << es[0].size() << " " << es[1].size() << endl;
  envs.clear();
  clock_t t_env=clock();
  float score;
  double m = LineOptimizer::LineOptimize(metric,es, LineOptimizer::MAXIMIZE_SCORE, &score);
  clock_t t_opt=clock();
  cerr << "line optimizer returned: " << m << " (SCORE=" << score << ")\n";
  BOOST_CHECK_CLOSE(0.48719698, score, 1e-5);
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
}

BOOST_AUTO_TEST_CASE(TestZeroOrigin) {
  const string json = "{\"rules\":[1,\"[X7] ||| blA ||| without ||| LHSProb=3.92173 LexE2F=2.90799 LexF2E=1.85003 GenerativeProb=10.5381 RulePenalty=1 XFE=2.77259 XEF=0.441833 LabelledEF=2.63906 LabelledFE=4.96981 LogRuleCount=0.693147\",2,\"[X7] ||| blA ||| except ||| LHSProb=4.92173 LexE2F=3.90799 LexF2E=1.85003 GenerativeProb=11.5381 RulePenalty=1 XFE=2.77259 XEF=1.44183 LabelledEF=2.63906 LabelledFE=4.96981 LogRuleCount=1.69315\",3,\"[S] ||| [X7,1] ||| [1] ||| GlueTop=1\",4,\"[X28] ||| EnwAn ||| title ||| LHSProb=3.96802 LexE2F=2.22462 LexF2E=1.83258 GenerativeProb=10.0863 RulePenalty=1 XFE=0 XEF=1.20397 LabelledEF=1.20397 LabelledFE=-1.98341e-08 LogRuleCount=1.09861\",5,\"[X0] ||| EnwAn ||| funny ||| LHSProb=3.98479 LexE2F=1.79176 LexF2E=3.21888 GenerativeProb=11.1681 RulePenalty=1 XFE=0 XEF=2.30259 LabelledEF=2.30259 LabelledFE=0 LogRuleCount=0 SingletonRule=1\",6,\"[X8] ||| [X7,1] EnwAn ||| entitled [1] ||| LHSProb=3.82533 LexE2F=3.21888 LexF2E=2.52573 GenerativeProb=11.3276 RulePenalty=1 XFE=1.20397 XEF=1.20397 LabelledEF=2.30259 LabelledFE=2.30259 LogRuleCount=0 SingletonRule=1\",7,\"[S] ||| [S,1] [X28,2] ||| [1] [2] ||| Glue=1\",8,\"[S] ||| [S,1] [X0,2] ||| [1] [2] ||| Glue=1\",9,\"[S] ||| [X8,1] ||| [1] ||| GlueTop=1\",10,\"[Goal] ||| [S,1] ||| [1]\"],\"features\":[\"PassThrough\",\"Glue\",\"GlueTop\",\"LanguageModel\",\"WordPenalty\",\"LHSProb\",\"LexE2F\",\"LexF2E\",\"GenerativeProb\",\"RulePenalty\",\"XFE\",\"XEF\",\"LabelledEF\",\"LabelledFE\",\"LogRuleCount\",\"SingletonRule\"],\"edges\":[{\"tail\":[],\"spans\":[0,1,-1,-1],\"feats\":[5,3.92173,6,2.90799,7,1.85003,8,10.5381,9,1,10,2.77259,11,0.441833,12,2.63906,13,4.96981,14,0.693147],\"rule\":1},{\"tail\":[],\"spans\":[0,1,-1,-1],\"feats\":[5,4.92173,6,3.90799,7,1.85003,8,11.5381,9,1,10,2.77259,11,1.44183,12,2.63906,13,4.96981,14,1.69315],\"rule\":2}],\"node\":{\"in_edges\":[0,1],\"cat\":\"X7\"},\"edges\":[{\"tail\":[0],\"spans\":[0,1,-1,-1],\"feats\":[2,1],\"rule\":3}],\"node\":{\"in_edges\":[2],\"cat\":\"S\"},\"edges\":[{\"tail\":[],\"spans\":[1,2,-1,-1],\"feats\":[5,3.96802,6,2.22462,7,1.83258,8,10.0863,9,1,11,1.20397,12,1.20397,13,-1.98341e-08,14,1.09861],\"rule\":4}],\"node\":{\"in_edges\":[3],\"cat\":\"X28\"},\"edges\":[{\"tail\":[],\"spans\":[1,2,-1,-1],\"feats\":[5,3.98479,6,1.79176,7,3.21888,8,11.1681,9,1,11,2.30259,12,2.30259,15,1],\"rule\":5}],\"node\":{\"in_edges\":[4],\"cat\":\"X0\"},\"edges\":[{\"tail\":[0],\"spans\":[0,2,-1,-1],\"feats\":[5,3.82533,6,3.21888,7,2.52573,8,11.3276,9,1,10,1.20397,11,1.20397,12,2.30259,13,2.30259,15,1],\"rule\":6}],\"node\":{\"in_edges\":[5],\"cat\":\"X8\"},\"edges\":[{\"tail\":[1,2],\"spans\":[0,2,-1,-1],\"feats\":[1,1],\"rule\":7},{\"tail\":[1,3],\"spans\":[0,2,-1,-1],\"feats\":[1,1],\"rule\":8},{\"tail\":[4],\"spans\":[0,2,-1,-1],\"feats\":[2,1],\"rule\":9}],\"node\":{\"in_edges\":[6,7,8],\"cat\":\"S\"},\"edges\":[{\"tail\":[5],\"spans\":[0,2,-1,-1],\"feats\":[],\"rule\":10}],\"node\":{\"in_edges\":[9],\"cat\":\"Goal\"}}";
  Hypergraph hg;
  istringstream instr(json);
  HypergraphIO::ReadFromJSON(&instr, &hg);
  SparseVector<double> wts;
  wts.set_value(FD::Convert("PassThrough"), -0.929201533002898);
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
 
  SparseVector<double> axis; axis.set_value(FD::Convert("Glue"),1.0);
  ConvexHullWeightFunction wf(wts, axis);  // wts = starting point, axis = search direction
  vector<ConvexHull> envs(1);
  envs[0] = Inside<ConvexHull, ConvexHullWeightFunction>(hg, NULL, wf);

  vector<vector<WordID> > mr(4);
  TD::ConvertSentence("untitled", &mr[0]);
  TD::ConvertSentence("with no title", &mr[1]);
  TD::ConvertSentence("without a title", &mr[2]);
  TD::ConvertSentence("without title", &mr[3]);
  EvaluationMetric* metric = EvaluationMetric::Instance("IBM_BLEU");
  boost::shared_ptr<SegmentEvaluator> scorer1 = metric->CreateSegmentEvaluator(mr);
  vector<ErrorSurface> es(1);
  ComputeErrorSurface(*scorer1, envs[0], &es[0], metric, hg);
}

