#ifndef HG_CFG_H
#define HG_CFG_H

#include "cfg.h"

class Hypergraph;

// in case you might want the CFG whether or not you apply FSA models:
struct HgCFG {
  HgCFG(Hypergraph const& ih) : ih(ih) { have_cfg=binarized=false; }
  Hypergraph const& ih;
  CFG cfg;
  bool have_cfg;
  void InitCFG(CFG &to) {
    to.Init(ih,true,false,true);
  }
  bool binarized;
  CFG &GetCFG()
  {
    if (!have_cfg) {
      have_cfg=true;
      InitCFG(cfg);
    }
    return cfg;
  }
  void GiveCFG(CFG &to) {
    if (!have_cfg)
      InitCFG(to);
    else {
      have_cfg=false;
      to.Clear();
      to.Swap(cfg);
    }
  }
  CFG const& GetCFG() const {
    assert(have_cfg);
    return cfg;
  }
};


#endif
