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
  int bin_thresh;
  bool bin_l2r;
  int bin_unary;
  bool bin_name_nts;
  bool bin_topo;
  bool bin_split;
  int split_passes,split_share1_passes,split_free_passes;
  template <class Opts> // template to support both printable_opts and boost nonprintable
  void AddOptions(Opts *opts) {
    opts->add_options()
      ("cfg_binarize_threshold", defaulted_value(&bin_thresh),"(if >0) repeatedly binarize CFG rhs bigrams which appear at least this many times, most frequent first.  resulting rules may be 1,2, or >2-ary.  this happens before the other types of binarization.")
//      ("cfg_binarize_unary_threshold", defaulted_value(&bin_unary),"if >0, a rule-completing production A->BC may be binarized as A->U U->BC if U->BC would be used at least this many times.  this happens last.")
      ("cfg_binarize_split", defaulted_value(&bin_split),"(DeNero et al) for each rule until binarized, pick a split point k of L->r[0..n) to make rules L->V1 V2, V1->r[0..k) V2->r[k..n), to minimize the number of new rules created")
      ("cfg_split_full_passes", defaulted_value(&split_passes),"pass through the virtual rules only (up to) this many times (all real rules will have been split if not already binary)")
      ("cfg_split_share1_passes", defaulted_value(&split_share1_passes),"after the full passes, for up to this many times split when at least 1 of the items has been seen before")
      ("cfg_split_free_passes", defaulted_value(&split_free_passes),"only split off from virtual nts pre/post nts that already exist - could check for interior phrases but after a few splits everything should be tiny already.")
      ("cfg_binarize_l2r", defaulted_value(&bin_l2r),"force left to right (a (b (c d))) binarization (ignore _at threshold)")
      ("cfg_binarize_name_nts", defaulted_value(&bin_name_nts),"create named virtual NT tokens e.g. 'A12+the' when binarizing 'B->[A12] the cat'")
      ("cfg_binarize_topo", defaulted_value(&bin_topo),"reorder nonterminals after binarization to maintain definition before use (topological order).  otherwise the virtual NTs will all appear after the regular NTs")
    ;
  }
  void Validate() {
    if (bin_thresh>0&&!bin_l2r) {
//      std::cerr<<"\nWARNING: greedy binarization not yet supported; using l2r (right branching) instead.\n";
//      bin_l2r=true;
    }
    if (false && bin_l2r && bin_split) { // actually, split may be slightly incomplete due to finite number of passes.
      std::cerr<<"\nWARNING: l2r and split are both complete binarization and redundant.  Using split.\n";
      bin_l2r=false;
    }

  }

  bool Binarizing() const {
    return bin_split || bin_l2r || bin_thresh>0;
  }
  void set_defaults() {
    bin_split=false;
    bin_topo=false;
    bin_thresh=0;
    bin_unary=0;
    bin_name_nts=true;
    bin_l2r=false;
    split_passes=10;split_share1_passes=0;split_free_passes=10;
  }
  CFGBinarize() { set_defaults(); }
  void print(std::ostream &o) const {
    o<<'(';
    if (!Binarizing())
      o << "Unbinarized";
    else {
      if (bin_unary)
        o << "unary-sharing ";
      if (bin_thresh)
        o<<"greedy bigram count>="<<bin_thresh<<" ";
      if (bin_l2r)
        o << "left->right";
      else
        o << "DeNero greedy split";
      if (bin_name_nts)
        o << " named-NTs";
      if (bin_topo)
        o<<" preserve-topo-order";
    }
    o<<')';
  }
  friend inline std::ostream &operator<<(std::ostream &o,CFGBinarize const& me) {
    me.print(o); return o;
  }

};


#endif
