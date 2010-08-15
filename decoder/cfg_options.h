#ifndef CFG_OPTIONS_H
#define CFG_OPTIONS_H

#include "hg_cfg.h"
#include "cfg_format.h"
#include "cfg_binarize.h"
//#include "program_options.h"

struct CFGOptions {
  CFGFormat format;
  CFGBinarize binarize;
  std::string out,source_out,unbin_out;
  void set_defaults() {
    format.set_defaults();
    binarize.set_defaults();
    out="";
  }
  CFGOptions() { set_defaults(); }
  template <class Opts> // template to support both printable_opts and boost nonprintable
  void AddOptions(Opts *opts) {
    opts->add_options()
      ("cfg_output", defaulted_value(&out),"write final target CFG (before FSA rescoring) to this file")
      ("source_cfg_output", defaulted_value(&source_out),"write source CFG (after prelm-scoring, prelm-prune) to this file")
      ("cfg_unbin_output", defaulted_value(&unbin_out),"write pre-binarization CFG to this file") //TODO:
    ;
    binarize.AddOptions(opts);
    format.AddOptions(opts);
  }
  void Validate() {
    format.Validate();
    binarize.Validate();
//    if (out.empty()) binarize.bin_name_nts=false;
  }
  char const* description() const {
    return "CFG output options";
  }
  void maybe_output_source(Hypergraph const& hg) {
    if (source_out.empty()) return;
    std::cerr<<"Printing source CFG to "<<source_out<<": "<<format<<'\n';
    WriteFile o(source_out);
    CFG cfg(hg,false,format.features,format.goal_nt());
    cfg.Print(o.get(),format);
  }
  void maybe_print(CFG &cfg,std::string cfg_output,char const* desc=" unbinarized") {
      WriteFile o(cfg_output);
      std::cerr<<"Printing target"<<desc<<" CFG to "<<cfg_output<<": "<<format<<'\n';
      cfg.Print(o.get(),format);
  }

  void maybe_output(HgCFG &hgcfg) {
    if (out.empty() && unbin_out.empty()) return;
    CFG &cfg=hgcfg.GetCFG();
    maybe_print(cfg,unbin_out);
    maybe_binarize(hgcfg);
    maybe_print(cfg,out,"");
  }

  void maybe_binarize(HgCFG &hgcfg) {
    if (hgcfg.binarized) return;
    CFG &cfg=hgcfg.GetCFG();
    cfg.Binarize(binarize);
    hgcfg.binarized=true;
  }
};


#endif
