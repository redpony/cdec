#ifndef CFG_OPTIONS_H
#define CFG_OPTIONS_H

#include "hg_cfg.h"
#include "cfg_format.h"
#include "cfg_binarize.h"
//#include "program_options.h"

struct CFGOptions {
  CFGFormat format;
  CFGBinarize binarize;
  std::string cfg_output;
  void set_defaults() {
    format.set_defaults();
    binarize.set_defaults();
    cfg_output="";
  }
  CFGOptions() { set_defaults(); }
  template <class Opts> // template to support both printable_opts and boost nonprintable
  void AddOptions(Opts *opts) {
    opts->add_options()
      ("cfg_output", defaulted_value(&cfg_output),"write final target CFG (before FSA rescorinn) to this file")
    ;
    binarize.AddOptions(opts);
    format.AddOptions(opts);
  }
  void Validate() {
    format.Validate();
    binarize.Validate();
  }
  char const* description() const {
    return "CFG output options";
  }
  void maybe_output(HgCFG &hgcfg) {
    if (cfg_output.empty()) return;
    WriteFile o(cfg_output);
    maybe_binarize(hgcfg);
    hgcfg.GetCFG().Print(o.get(),format);
  }
  void maybe_binarize(HgCFG &hgcfg) {
    if (hgcfg.binarized) return;
    hgcfg.GetCFG().Binarize(binarize);
    hgcfg.binarized=true;
  }

};


#endif
