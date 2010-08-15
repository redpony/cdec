#ifndef CFG_FORMAT_H
#define CFG_FORMAT_H

#include <iostream>
#include <string>
#include "wordid.h"
#include "feature_vector.h"
#include "program_options.h"

struct CFGFormat {
  bool identity_scfg;
  bool features;
  bool logprob_feat;
  bool comma_nt;
  bool nt_span;
  std::string goal_nt_name;
  std::string nt_prefix;
  std::string logprob_feat_name;
  std::string partsep;
  bool goal_nt() const { return !goal_nt_name.empty(); }
  template <class Opts> // template to support both printable_opts and boost nonprintable
  void AddOptions(Opts *opts) {
    //using namespace boost::program_options;
    //using namespace std;
    opts->add_options()
      ("identity_scfg",defaulted_value(&identity_scfg),"output an identity SCFG: add an identity target side - '[X12] ||| [X13,1] a ||| [1] a ||| feat= ...' - the redundant target '[1] a |||' is omitted otherwise.")
      ("features",defaulted_value(&features),"print the CFG feature vector")
      ("logprob_feat",defaulted_value(&logprob_feat),"print a LogProb=-1.5 feature irrespective of --features.")
      ("logprob_feat_name",defaulted_value(&logprob_feat_name),"alternate name for the LogProb feature")
      ("cfg_comma_nt",defaulted_value(&comma_nt),"if false, omit the usual [NP,1] ',1' variable index in the source side")
      ("goal_nt_name",defaulted_value(&goal_nt_name),"if nonempty, the first production will be '[goal_nt_name] ||| [x123] ||| LogProb=y' where x123 is the actual goal nt, and y is the pushed prob, if any")
      ("nt_prefix",defaulted_value(&nt_prefix),"NTs are [<nt_prefix>123] where 123 is the node number starting at 0, and the highest node (last in file) is the goal node in an acyclic hypergraph")
      ("nt_span",defaulted_value(&nt_span),"prefix A(i,j) for NT coming from hypergraph node with category A on span [i,j).  this is after --nt_prefix if any")
      ;
  }

  void print(std::ostream &o) const {
    o<<"[";
    if (identity_scfg)
      o<<"Identity SCFG ";
    if (features)
      o<<"+Features ";
    if (logprob_feat)
      o<<logprob_feat_name<<"(logprob) ";
    if (nt_span)
      o<<"named-NTs ";
    if (comma_nt)
      o<<",N ";
    o << "CFG output format";
    o<<"]";
  }
  friend inline std::ostream &operator<<(std::ostream &o,CFGFormat const& me) {
    me.print(o); return o;
  }

  void Validate() {  }
  template<class CFG>
  void print_source_nt(std::ostream &o,CFG const&cfg,int id,int position=1) const {
    o<<'[';
    print_nt_name(o,cfg,id);
    if (comma_nt) o<<','<<position;
    o<<']';
  }

  template <class CFG>
  void print_nt_name(std::ostream &o,CFG const& cfg,int id) const {
    o<<nt_prefix;
    if (nt_span)
      cfg.print_nt_name(o,id);
    else
      o<<id;
  }

  template <class CFG>
  void print_lhs(std::ostream &o,CFG const& cfg,int id) const {
    o<<'[';
    print_nt_name(o,cfg,id);
    o<<']';
  }

  template <class CFG,class Iter>
  void print_rhs(std::ostream &o,CFG const&cfg,Iter begin,Iter end) const {
    o<<partsep;
    int pos=0;
    for (Iter i=begin;i!=end;++i) {
      WordID w=*i;
      if (i!=begin) o<<' ';
      if (w>0) o << TD::Convert(w);
      else print_source_nt(o,cfg,-w,++pos);
    }
    if (identity_scfg) {
      o<<partsep;
      int pos=0;
      for (Iter i=begin;i!=end;++i) {
        WordID w=*i;
        if (i!=begin) o<<' ';
        if (w>0) o << TD::Convert(w);
        else o << '['<<++pos<<']';
      }
    }
  }

  void print_features(std::ostream &o,prob_t p,FeatureVector const& fv=FeatureVector()) const {
    bool logp=(logprob_feat && p!=1);
    if (features || logp) {
      o << partsep;
      if (logp)
        o << logprob_feat_name<<'='<<log(p)<<' ';
      if (features)
        o << fv;
    }
  }

  //TODO: default to no nt names (nt_span=0)
  void set_defaults() {
    identity_scfg=false;
    features=true;
    logprob_feat=true;
    comma_nt=true;
    goal_nt_name="S";
    logprob_feat_name="LogProb";
    nt_prefix="";
    partsep=" ||| ";
    nt_span=true;
  }

  CFGFormat() {
    set_defaults();
  }
};



#endif
