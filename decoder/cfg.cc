#include "cfg.h"
#include "hg.h"
#include "cfg_format.h"
#include "cfg_binarize.h"

using namespace std;


void CFG::Binarize(CFGBinarize const& b) {
  if (!b.Binarizing()) return;
  if (!b.bin_l2r) {
    assert(b.bin_l2r);
    return;
  }
  // l2r only so far:
  cerr << "Binarizing "<<b<<endl;
  //TODO.
}

namespace {
inline int nt_index(int nvar,Hypergraph::TailNodeVector const& t,bool target_side,int w) {
  assert(w<0 || (target_side&&w==0));
  return t[target_side?-w:nvar];
}
}

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
    prob_t &crp=cfgr.p;
    crp=e.edge_prob_;
    cfgr.lhs=e.head_node_;
#if CFG_DEBUG
    cfgr.rule=e.rule_;
#endif
    if (copy_features) cfgr.f=e.feature_values_;
    if (push_weights) crp /=np[e.head_node_];
    TRule const& er=*e.rule_;
    vector<WordID> const& rule_rhs=target_side?er.e():er.f();
    int nr=rule_rhs.size();
    RHS &rhs_out=cfgr.rhs;
    rhs_out.resize(nr);
    Hypergraph::TailNodeVector const& tails=e.tail_nodes_;
    int nvar=0;
    //split out into separate target_side, source_side loops?
    for (int j=0;j<nr;++j) {
      WordID w=rule_rhs[j];
      if (w>0)
        rhs_out[j]=w;
      else {
        int n=nt_index(nvar,tails,target_side,w);
        ++nvar;
        if (push_weights) crp*=np[n];
        rhs_out[j]=n;
      }
    }
    assert(nvar==er.Arity());
    assert(nvar==tails.size());
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
#if CFG_DEBUG
  o<<f.partsep<<*r.rule;
#endif
}

void CFG::Print(std::ostream &o,CFGFormat const& f) const {
  assert(!uninit);
  if (!f.goal_nt_name.empty()) {
    o << '['<<f.goal_nt_name <<']';
    WordID rhs=-goal_nt;
    f.print_rhs(o,*this,&rhs,&rhs+1);
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
