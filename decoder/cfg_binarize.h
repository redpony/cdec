#ifndef CFG_BINARIZE_H
#define CFG_BINARIZE_H

#include <iostream>

/*
  binarization: decimate rhs of original rules until their rhs have been reduced to length 2 (or 1 if bin_unary).  also decimate rhs of newly binarized rules until length 2.  newly created rules are all binary (never unary/nullary).

  bin_name_nts: nts[i].from will be initialized, including adding new names to TD

  bin_l2r: right-branching (a (b c)) means suffixes are shared.  if requested, the only other option that matters is bin_unary

  otherwise, greedy binarization: the pairs that are most frequent in the rules are binarized, one at a time.  this should be done efficiently: each pair has a count of and list of its left and right adjacent pair+count (or maybe a non-count collapsed list of adjacent instances).  this can be efficiently updated when a pair is chosen for replacement by a new virtual NT.
 */

struct CFGBinarize {
  int bin_at;
  bool bin_l2r;
  bool bin_unary;
  bool bin_name_nts;
  template <class Opts> // template to support both printable_opts and boost nonprintable
  void AddOptions(Opts *opts) {
    opts->add_options()
      ("cfg_binarize_at", defaulted_value(&bin_at),"(if >0) binarize CFG rhs segments which appear at least this many times")
      ("cfg_binarize_unary", defaulted_value(&bin_unary),"if true, a rule-completing production A->BC may be binarized as A->U U->BC if U->BC would be used at least cfg_binarize_at times.")
      ("cfg_binarize_l2r", defaulted_value(&bin_l2r),"force left to right (a (b (c d))) binarization (ignore _at threshold)")
      ("cfg_binarize_name_nts", defaulted_value(&bin_name_nts),"create named virtual NT tokens e.g. 'A12+the' when binarizing 'B->[A12] the cat'")
    ;
  }
  void Validate() {
    if (bin_l2r)
      bin_at=0;
    if (bin_at>0&&!bin_l2r) {
      std::cerr<<"\nWARNING: greedy binarization not yet supported; using l2r (right branching) instead.\n";
      bin_l2r=true;
    }
  }

  bool Binarizing() const {
    return bin_l2r || bin_at>0;
  }
  void set_defaults() {
    bin_at=0;
    bin_unary=false;
    bin_name_nts=true;
    bin_l2r=false;
  }
  CFGBinarize() { set_defaults(); }
  void print(std::ostream &o) const {
    o<<'(';
    if (!Binarizing())
      o << "Unbinarized";
    else {
      if (bin_unary)
        o << "unary-sharing ";
      if (bin_l2r)
        o << "left->right";
      else
        o << "greedy count>="<<bin_at;
      if (bin_name_nts)
        o << " named-NTs";
    }
    o<<')';
  }
  friend inline std::ostream &operator<<(std::ostream &o,CFGBinarize const& me) {
    me.print(o); return o;
  }

};


#endif
