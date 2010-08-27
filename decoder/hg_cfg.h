#ifndef HG_CFG_H
#define HG_CFG_H

#include "cfg.h"

class Hypergraph;

// in case you might want the CFG whether or not you apply FSA models:
struct HgCFG {
  void set_defaults() {
    have_cfg=binarized=have_features=uniqed=false;
    want_features=true;
  }
  HgCFG(Hypergraph const& ih) : ih(ih) {
    set_defaults();
  }
  Hypergraph const& ih;
  CFG cfg;
  bool have_cfg;
  bool have_features;
  bool want_features;
  void InitCFG(CFG &to) {
    to.Init(ih,true,want_features,true);
  }
  bool binarized;
  bool uniqed;
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
      cfg.VisitRuleIds(*this);
      have_cfg=false;
      to.Clear();
      swap(to,cfg);
    }
  }
  void operator()(int ri) const {
  }
  CFG const& GetCFG() const {
    assert(have_cfg);
    return cfg;
  }
};


#endif
