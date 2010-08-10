#include "cfg.h"
#include "hg.h"
#include "cfg_format.h"

using namespace std;

void CFG::Init(Hypergraph const& hg,bool target_side,bool copy_features,bool push_weights) {
  uninit=false;
  hg_=&hg;
  Hypergraph::NodeProbs np;
  goal_inside=hg.ComputeNodeViterbi(&np);
  pushed_inside=push_weights ? goal_inside : prob_t(1);
  int nn=hg.nodes_.size(),ne=hg.edges_.size();
  nts.resize(nn);
  goal_nt=nn-1;
  rules.resize(ne);
  for (int i=0;i<nn;++i) {
    nts[i].ruleids=hg.nodes_[i].in_edges_;
    hg.SetNodeOrigin(i,nts[i].from);
  }
  for (int i=0;i<ne;++i) {
    Rule &cfgr=rules[i];
    Hypergraph::Edge const& e=hg.edges_[i];
    TRule const& er=*e.rule_; vector<WordID> const& rule_rhs=target_side?er.e():er.f();
    RHS &rhs=cfgr.rhs;
    int nr=rule_rhs.size();
    rhs.resize(nr);
    prob_t &crp=cfgr.p;
    crp=e.edge_prob_;
    cfgr.lhs=e.head_node_;
#if CFG_DEBUG
    cfgr.rule=e.rule_;
#endif
    if (copy_features) cfgr.f=e.feature_values_;
    if (push_weights) crp /=np[e.head_node_];
    for (int j=0;j<nr;++j) {
      WordID w=rule_rhs[j];
      if (w>0)
        rhs[j]=w;
      else {
        int n=e.tail_nodes_[-w];
        if (push_weights) crp*=np[n];
        rhs[j]=n;
      }
    }
  }
}

void CFG::Clear() {
  rules.clear();
  nts.clear();
  goal_nt=-1;
  hg_=0;
}

void CFG::PrintRule(std::ostream &o,RuleHandle rulei,CFGFormat const& f) const {
  Rule const& r=rules[rulei];
  f.print_lhs(o,*this,r.lhs);
  f.print_rhs(o,*this,r.rhs.begin(),r.rhs.end());
  f.print_features(o,r.p,r.f);
}

void CFG::Print(std::ostream &o,CFGFormat const& f) const {
  assert(!uninit);
  if (!f.goal_nt_name.empty()) {
    o << '['<<f.goal_nt_name <<']';
    f.print_rhs(o,*this,&goal_nt,&goal_nt+1);
    if (pushed_inside!=1)
      f.print_features(o,pushed_inside);
    o<<'\n';
  }
  for (int i=0;i<nts.size();++i) {
    Ruleids const& ntr=nts[i].ruleids;
    for (Ruleids::const_iterator j=ntr.begin(),jj=ntr.end();j!=jj;++j) {
      PrintRule(o,*j,f);
      o<<'\n';
    }
  }
}
