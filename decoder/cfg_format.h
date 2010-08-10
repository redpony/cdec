#ifndef CFG_FORMAT_H
#define CFG_FORMAT_H

#include <program_options.h>
#include <string>

struct CFGFormat {
  bool identity_scfg;bool features;bool logprob_feat;bool cfg_comma_nt;std::string goal_nt_name;std::string nt_prefix;
  template <class Opts> // template to support both printable_opts and boost nonprintable
  void AddOptions(Opts *opts) {
    using namespace boost::program_options;
    using namespace std;
    opts->add_options()
      ("identity_scfg",defaulted_value(&identity_scfg),"output an identity SCFG: add an identity target side - '[X12] ||| [X13,1] a ||| [1] a ||| feat= ...' - the redundant target '[1] a |||' is omitted otherwise.")
      ("features",defaulted_value(&features),"print the CFG feature vector")
      ("logprob_feat",defaulted_value(&logprob_feat),"print a LogProb=-1.5 feature irrespective of --features.")
      ("cfg_comma_nt",defaulted_value(&cfg_comma_nt),"if false, omit the usual [NP,1] ',1' variable index in the source side")
      ("goal_nt_name",defaulted_value(&goal_nt_name),"if nonempty, the first production will be '[goal_nt_name] ||| [x123] ||| LogProb=y' where x123 is the actual goal nt, and y is the pushed prob, if any")
      ("nt_prefix",defaulted_value(&nt_prefix),"NTs are [<nt_prefix>123] where 123 is the node number starting at 0, and the highest node (last in file) is the goal node in an acyclic hypergraph")
      ;
  }
  void set_defaults() {
    identity_scfg=false;
    features=true;
    logprob_feat=true;
    cfg_comma_nt=true;
    goal_nt_name="S";
    nt_prefix="";
  }
  CFGFormat() {
    set_defaults();
  }
};


#endif
