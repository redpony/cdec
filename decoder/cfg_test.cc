#include <gtest/gtest.h>
#include "cfg.h"
#include "hg_test.h"
#include "cfg_options.h"

#define CSHOW_V 1
#if CSHOW_V
# define CSHOWDO(x) x
#else
# define CSHOWDO(x)
#endif
#define CSHOW(x) CSHOWDO(cerr<<#x<<'='<<x<<endl;)

struct CFGTest : public HGSetup {
  CFGTest() {  }
  ~CFGTest() {  }
  static void JsonFN(Hypergraph hg,CFG &cfg,std::string file
                     ,std::string const& wts="Model_0 1 EgivenF 1 f1 1")
  {
    FeatureVector featw;
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
};

TEST_F(CFGTest,Binarize) {
  Hypergraph hg;
  CFG cfg;
  JsonFN(hg,cfg,perro_json,perro_wts);
  CSHOW("\nCFG Test.\n");
  CFGBinarize b;
  CFGFormat form;
  form.nt_span=true;
  for (int i=-1;i<16;++i) {
    b.bin_l2r=i>=0;
    b.bin_unary=i&1;
    b.bin_name_nts=i&2;
    b.bin_uniq=i&4;
    b.bin_topo=i&8;
    CFG cc=cfg;
    EXPECT_EQ(cc,cfg);
    CSHOW("\nBinarizing: "<<b);
    cc.Binarize(b);
    CSHOWDO(cc.Print(cerr,form);cerr<<"\n\n";);
  }
}

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
