#include "cfg.h"
#include "hg.h"
#include "cfg_format.h"
#include "cfg_binarize.h"
#include "hash.h"
#include "batched_append.h"
#include <limits>
#include "fast_lexical_cast.hpp"
//#include "indices_after.h"

#define CFGPRINT(x) IF_CFG_DEBUG(std::cerr<<x)
#define CFGSHOWC(x,s) CFGPRINT(#x<<"="<<x<<s)
#define CFGSHOW(x) CFGSHOWC(x,"\n")
#define CFGSHOWS(x) CFGSHOWC(x," ")
#define CFGSHOW2(x,y) CFGSHOWS(x) CFGSHOW(y)

using namespace std;

typedef CFG::Rule Rule;
typedef CFG::NTOrder NTOrder;
typedef CFG::RHS RHS;

/////index ruleids:
void CFG::UnindexRules() {
  for (NTs::iterator n=nts.begin(),nn=nts.end();n!=nn;++n)
    n->ruleids.clear();
}

void CFG::ReindexRules() {
  UnindexRules();
  for (int i=0,e=rules.size();i<e;++i)
    if (!rules[i].is_null())
      nts[rules[i].lhs].ruleids.push_back(i);
}

//////topo order:
namespace {
typedef std::vector<char> Seen; // 0 = unseen, 1 = seen+finished, 2 = open (for cycle detection; seen but not finished)
enum { UNSEEN=0,SEEN,OPEN };


// bottom -> top topo order (rev head->tails topo)
template <class OutOrder>
struct CFGTopo {
// meaningless efficiency alternative: close over all the args except ni - so they're passed as a single pointer.  also makes visiting tail_nts simpler.
  CFG const& cfg;
  OutOrder outorder;
  std::ostream *cerrp;
  CFGTopo(CFG const& cfg,OutOrder const& outorder,std::ostream *cerrp=&std::cerr)
    : cfg(cfg),outorder(outorder),cerrp(cerrp) // closure over args
    , seen(cfg.nts.size()) {  }

  Seen seen;
  void operator()(CFG::NTHandle ni) {
    char &seenthis=seen[ni];
    if (seenthis==UNSEEN) {
      seenthis=OPEN;

      CFG::NT const& nt=cfg.nts[ni];
      for (CFG::Ruleids::const_iterator i=nt.ruleids.begin(),e=nt.ruleids.end();i!=e;++i) {
        Rule const& r=cfg.rules[*i];
        r.visit_rhs_nts(*this); // recurse.
      }

      *outorder++=ni; // dfs finishing time order = reverse topo.
      seenthis=SEEN;
    } else if (cerrp && seenthis==OPEN) {
      std::ostream &cerr=*cerrp;
      cerr<<"WARNING: CFG Topo order attempt failed: NT ";
      cfg.print_nt_name(cerr,ni);
      cerr<<" already reached from goal(top) ";
      cfg.print_nt_name(cerr,cfg.goal_nt);
      cerr<<".  Continuing to reorder, but it's not fully topological.\n";
    }
  }

};

template <class O>
void DoCFGTopo(CFG const& cfg,CFG::NTHandle goal,O const& o,std::ostream *w=0) {
  CFGTopo<O> ct(cfg,o,w);
  ct(goal);
}

}//ns

// you would need to do this only if you didn't build from hg, or you Binarize without bin_topo option.  note: this doesn't sort the list of rules; it's assumed that if you care about the topo order you'll iterate over nodes.
void CFG::OrderNTsTopo(NTOrder *o_,std::ostream *cycle_complain) {
  NTOrder &o=*o_;
  o.resize(nts.size());
  DoCFGTopo(*this,goal_nt,o.begin(),cycle_complain);
}


/////sort/uniq:
namespace {
RHS null_rhs(1,INT_MIN);

//sort
struct ruleid_best_first {
  CFG::Rules const* rulesp;
  bool operator()(int a,int b) const { // true if a >(prob for ruleid) b
    return (*rulesp)[b].p < (*rulesp)[a].p;
  }
};

//uniq
struct prob_pos {
  prob_pos() {}
  prob_pos(prob_t prob,int pos) : prob(prob),pos(pos) {}
  prob_t prob;
  int pos;
  bool operator <(prob_pos const& o) const { return prob<o.prob; }
};
}//ns

int CFG::UniqRules(NTHandle ni) {
  typedef HASH_MAP<RHS,prob_pos,boost::hash<RHS> > BestRHS; // faster to use trie? maybe.
  BestRHS bestp; // once inserted, the position part (output index) never changes.  but the prob may be improved (overwrite ruleid at that position).
  HASH_MAP_EMPTY(bestp,null_rhs);
  Ruleids &adj=nts[ni].ruleids;
  Ruleids oldadj=adj;
  int oi=0;
  for (int i=0,e=oldadj.size();i!=e;++i) { // this beautiful complexity is to ensure that adj' is a subsequence of adj (without duplicates)
    int ri=oldadj[i];
    Rule const& r=rules[ri];
    prob_pos pi(r.p,oi);
    prob_pos &oldpi=get_default(bestp,r.rhs,pi);
    if (oldpi.pos==oi) {// newly inserted
      adj[oi++]=ri;
    } else if (oldpi.prob<pi.prob) { // we improve prev. best (overwrite it @old pos)
      oldpi.prob=pi.prob;
      adj[oldpi.pos]=ri; // replace worse rule w/ better
    }
  }
  // post: oi = number of new adj
  adj.resize(oi);
  return oi;
}

void CFG::SortLocalBestFirst(NTHandle ni) {
  ruleid_best_first r;
  r.rulesp=&rules;
  Ruleids &adj=nts[ni].ruleids;
  std::stable_sort(adj.begin(),adj.end(),r);
}


/////binarization:
namespace {

CFG::BinRhs null_bin_rhs(std::numeric_limits<int>::min(),std::numeric_limits<int>::min());

// index i >= N.size()?  then it's in M[i-N.size()]
//WordID first,WordID second,
string BinStr(CFG::BinRhs const& b,CFG::NTs const& N,CFG::NTs const& M)
{
  int nn=N.size();
  ostringstream o;
#undef BinNameOWORD
#define BinNameOWORD(w)                                 \
  do {                                                  \
    int n=w; if (n>0) o << TD::Convert(n);              \
    else {                                              \
      int i=-n;                                         \
      if (i<nn) o<<N[i].from<<i; else o<<M[i-nn].from;  \
    }                                                   \
  } while(0)

  BinNameOWORD(b.first);
  o<<'+';
  BinNameOWORD(b.second);
  return o.str();
}

WordID BinName(CFG::BinRhs const& b,CFG::NTs const& N,CFG::NTs const& M)
{
  return TD::Convert(BinStr(b,N,M));
}

}//ns

void CFG::Binarize(CFGBinarize const& b) {
  if (!b.Binarizing()) return;
  if (!b.bin_l2r) {
    assert(b.bin_l2r);
    return;
  }
  //TODO: l2r only so far:
  cerr << "Binarizing "<<b<<endl;
  HASH_MAP<BinRhs,NTHandle,boost::hash<BinRhs> > bin2lhs; // we're going to hash cons rather than build an explicit trie from right to left.
  HASH_MAP_EMPTY(bin2lhs,null_bin_rhs);
  // iterate using indices and not iterators because we'll be adding to both nts and rules list?  we could instead pessimistically reserve space for both, but this is simpler.  also: store original end of nts since we won't need to reprocess newly added ones.
  int rhsmin=b.bin_unary?0:1;
  NTs new_nts; // these will be appended at the end, so we don't have to worry about iterator invalidation
  Rules new_rules;
  //TODO: this could be factored easily into in-place (append to new_* like below) and functional (nondestructive copy) versions (copy orig to target and append to target)
  int newnt=-nts.size(); // we're going to store binary rhs with -nt to keep distinct from words (>=0)
  int newruleid=rules.size();
  BinRhs bin;
  for (NTs::const_iterator n=nts.begin(),nn=nts.end();n!=nn;++n) {
    NT const& nt=*n;
    for (Ruleids::const_iterator ir=nt.ruleids.begin(),er=nt.ruleids.end();ir!=er;++ir) {
      CFGPRINT("Rule id# ") CFGSHOWS(*ir);IF_CFG_DEBUG(PrintRule(cerr<<" '",*ir,CFGFormat());cerr<<"'\n");
      RHS &rhs=rules[*ir].rhs; // we're going to binarize this while adding newly created rules to new_...
      if (rhs.empty()) continue;
      int r=rhs.size()-2; // loop below: [r,r+1) is to be reduced into a (maybe new) binary NT
      if (rhsmin<=r) { // means r>=0 also
        bin.second=rhs[r+1];
        int bin_to; // the replacement for bin
        assert(newruleid==rules.size()+new_rules.size());assert(-newnt==nts.size()+new_nts.size());
        // also true at start/end of loop:
        for (;;) { // pairs from right to left (normally we leave the last pair alone)

          bin.first=rhs[r];
          bin_to=get_default(bin2lhs,bin,newnt);
          CFGSHOWS(r) CFGSHOWS(newnt) CFGPRINT("bin="<<BinStr(bin,nts,new_nts)<<"=>") CFGSHOW(bin_to);
          if (newnt==bin_to) { // it's new!
            new_nts.push_back(NT(newruleid++));
            //now -newnt is the index of the last (after new_nts is appended) nt.  bin is its rhs.  bin_to is its lhs
            new_rules.push_back(Rule(-newnt,bin));
            --newnt;
            if (b.bin_name_nts)
              new_nts.back().from.nt=BinName(bin,nts,new_nts);
          }
          bin.second=bin_to;
          --r;
          if (r<rhsmin) break;
        }
        rhs[rhsmin]=bin_to;
        rhs.resize(rhsmin+1);
      }
    }
  }
#if 0
  batched_append_swap(nts,new_nts);
  batched_append_swap(rules,new_rules);
#else
  batched_append(nts,new_nts);
  batched_append(rules,new_rules);
#endif
  if (b.bin_topo) //TODO: more efficient (at least for l2r) maintenance of order
    OrderNTsTopo();
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
    IF_CFG_TRULE(cfgr.rule=e.rule_;)
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
  IF_CFG_TRULE(if (r.rule) o<<f.partsep<<*r.rule;)
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

void CFG::Print(std::ostream &o) const {
  Print(o,CFGFormat());
}


std::ostream &operator<<(std::ostream &o,CFG const &x) {
  x.Print(o);
  return o;
}
