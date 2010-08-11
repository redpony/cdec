#include "cfg.h"
#include "hg.h"
#include "cfg_format.h"
#include "cfg_binarize.h"
#include "hash.h"
#include "batched_append.h"
#include <limits>
#include "fast_lexical_cast.hpp"

using namespace std;

namespace {
CFG::BinRhs nullrhs(std::numeric_limits<int>::min(),std::numeric_limits<int>::min());
}


WordID CFG::BinName(BinRhs const& b)
{
  ostringstream o;
#define BinNameOWORD(w) do { int n=w; if (n>0) o << TD::Convert(n); else { o << 'V' << -n; } } while(0)
  BinNameOWORD(b.first);
  o<<'+';
  BinNameOWORD(b.second);
#undef BinNameOWORD
  return TD::Convert(o.str());
}

void CFG::Binarize(CFGBinarize const& b) {
  if (!b.Binarizing()) return;
  if (!b.bin_l2r) {
    assert(b.bin_l2r);
    return;
  }
  // l2r only so far:
  cerr << "Binarizing "<<b<<endl;
  HASH_MAP<BinRhs,NTHandle,boost::hash<BinRhs> > bin2lhs; // we're going to hash cons rather than build an explicit trie from right to left.
  HASH_MAP_EMPTY(bin2lhs,nullrhs);
  int rhsmin=b.bin_unary?0:1;
  // iterate using indices and not iterators because we'll be adding to both nodes and edge list.  we could instead pessimistically reserve space for both, but this is simpler.  also: store original end of nts since we won't need to reprocess newly added ones.
  NTs new_nts; // these will be appended at the end, so we don't have to worry about iterator invalidation
  Rules new_rules;
  //TODO: this could be factored easily into in-place (append to new_* like below) and functional (nondestructive copy) versions (copy orig to target and append to target)
  int newnt=nts.size();
  int newruleid=rules.size();
  BinRhs bin;
  for (NTs::const_iterator n=nts.begin(),nn=nts.end();n!=nn;++n) {
    NT const& nt=*n;
    for (Ruleids::const_iterator ir=nt.ruleids.begin(),er=nt.ruleids.end();ir!=er;++ir) {
      RHS &rhs=rules[*ir].rhs; // we're going to binarize this while adding newly created rules to new_...
      if (rhs.empty()) continue;
      bin.second=rhs.back();
      for (int r=rhs.size()-2;r>=rhsmin;--r) { // pairs from right to left (normally we leave the last pair alone)
        rhs.pop_back();
        bin.first=rhs[r];
        if (newnt==(bin.second=(get_default(bin2lhs,bin,newnt)))) {
          new_nts.push_back(NT());
          new_nts.back().ruleids.push_back(newruleid);
          new_rules.push_back(Rule(newnt,bin));
          if (b.bin_name_nts)
            new_nts.back().from.nt=BinName(bin);
          ++newnt;++newruleid;
        }
      }
    }
  }
  batched_append_swap(nts,new_nts);
  batched_append_swap(rules,new_rules);
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
        rhs_out[j]=-n;
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
  if (r.rule) o<<f.partsep<<*r.rule;
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
