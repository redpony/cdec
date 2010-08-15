#include <boost/tuple/tuple.hpp>
#include <gtest/gtest.h>
#include "cfg.h"
#include "hg_test.h"
#include "cfg_options.h"
#include "show.h"

/* TODO: easiest way to get meaningful confirmations that things work: implement conversion back to hg, and compare viterbi/inside etc. stats for equality to original hg.  or you can define CSHOW_V and see lots of output */

using namespace boost;

#define CSHOW_V 0

#if CSHOW_V
# define CSHOWDO(x) x;
#else
# define CSHOWDO(x)
#endif
#define CSHOW(x) CSHOWDO(cerr<<#x<<'='<<x<<endl;)

typedef std::pair<string,string> HgW; // hg file,weights

struct CFGTest : public TestWithParam<HgW> {
  string hgfile;
  Hypergraph hg;
  CFG cfg;
  CFGFormat form;
  FeatureVector weights;

  static void JsonFN(Hypergraph &hg,CFG &cfg,FeatureVector &featw,std::string file
                     ,std::string const& wts="Model_0 1 EgivenF 1 f1 1")
  {
    istringstream ws(wts);
    EXPECT_TRUE(ws>>featw);
    CSHOW(featw)
    HGSetup::JsonTestFile(&hg,file);
    hg.Reweight(featw);
    cfg.Init(hg,true,true,false);
  }
  static void SetUpTestCase() {
  }
  static void TearDownTestCase() {
  }
  CFGTest() {
    hgfile=GetParam().first;
    JsonFN(hg,cfg,weights,hgfile,GetParam().second);
    CSHOWDO(cerr<<"\nCFG Test: ")
    CSHOW(hgfile);
    form.nt_span=true;
    form.comma_nt=false;
  }
  ~CFGTest() {  }
};

TEST_P(CFGTest,Binarize) {
  CFGBinarize b;
  b.bin_name_nts=1;
  CFG cfgu=cfg;
  EXPECT_EQ(cfgu,cfg);
  int nrules=cfg.rules.size();
  CSHOWDO(cerr<<"\nUniqing: "<<nrules<<"\n");
  int nrem=cfgu.UniqRules();
  cerr<<"\nCFG "<<hgfile<<" Uniqed - remaining: "<<nrem<<" of "<<nrules<<"\n";
  if (nrem==nrules) {
    EXPECT_EQ(cfgu,cfg);
    //TODO - check that 1best is still the same (that we removed only worse edges)
  }

  for (int i=-1;i<8;++i) {
    bool uniq;
    if (i>=0) {
      int f=i<<1;
      b.bin_l2r=1;
      b.bin_unary=(f>>=1)&1;
      b.bin_topo=(f>>=1)&1;
      uniq=(f>>=1)&1;
    } else
      b.bin_l2r=0;
    CFG cc=uniq?cfgu:cfg;
    CSHOW("\nBinarizing "<<(uniq?"uniqued ":"")<<": "<<i<<" "<<b);
    cc.Binarize(b);
    cerr<<"Binarized "<<b<<" rules size "<<cfg.rules_size()<<" => "<<cc.rules_size()<<"\n";
    CSHOWDO(cc.Print(cerr,form);cerr<<"\n\n";);
  }
}

INSTANTIATE_TEST_CASE_P(HypergraphsWeights,CFGTest,
                        Values(
                          HgW(perro_json,perro_wts)
                          , HgW(small_json,small_wts)
                            ,HgW(urdu_json,urdu_wts)
                          ));

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
