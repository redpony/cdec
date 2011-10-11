#ifndef _REACHABILITY_H_
#define _REACHABILITY_H_

#include "boost/multi_array.hpp"

// determines minimum and maximum lengths of outgoing edges from all
// coverage positions such that the alignment path respects src and
// trg maximum phrase sizes
//
// runs in O(n^2 * src_max * trg_max) time but should be relatively fast
//
// currently forbids 0 -> n and n -> 0 alignments

struct Reachability {
  boost::multi_array<bool, 4> edges;  // edges[src_covered][trg_covered][x][trg_delta] is this edge worth exploring?
  boost::multi_array<short, 2> max_src_delta; // msd[src_covered][trg_covered] -- the largest src delta that's valid

  Reachability(int srclen, int trglen, int src_max_phrase_len, int trg_max_phrase_len) :
      edges(boost::extents[srclen][trglen][src_max_phrase_len+1][trg_max_phrase_len+1]),
      max_src_delta(boost::extents[srclen][trglen]) {
    ComputeReachability(srclen, trglen, src_max_phrase_len, trg_max_phrase_len);
  }

 private:
  void ComputeReachability(int srclen, int trglen, int src_max_phrase_len, int trg_max_phrase_len);
};

#endif
