#include <gtest/gtest.h>
#include "cfg.h"
#include "hg_test.h"
#include "cfg_options.h"

struct CFGTest : public HGSetup {
  CFGTest() {  }
  ~CFGTest() {  }
  static void JsonFN(Hypergraph hg,CFG &cfg,std::string file
                     ,std::string const& wts="Model_0 1 EgivenF 1 f1 1")
  {
    FeatureVector v;
    istringstream ws(wts);
//    ASSERT_TRUE(ws>>v);
    HGSetup::JsonTestFile(&hg,file);
//    hg.Reweight(v);
    cfg.Init(hg,true,false,false);
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
  CFGFormat form;
  form.features=true;
  cerr<<"\nCFG Test.\n\n";
  cfg.Print(cerr,form);
}

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
