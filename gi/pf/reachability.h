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
  unsigned nodes;
  boost::multi_array<bool, 4> edges;  // edges[src_covered][trg_covered][src_delta][trg_delta] is this edge worth exploring?
  boost::multi_array<short, 2> max_src_delta; // msd[src_covered][trg_covered] -- the largest src delta that's valid
  boost::multi_array<short, 2> node_addresses; // na[src_covered][trg_covered] -- the index of the node in a one-dimensional array (of size "nodes")
  boost::multi_array<std::vector<std::pair<short,short> >, 2> valid_deltas; // valid_deltas[src_covered][trg_covered] list of valid transitions leaving a particular node

  Reachability(int srclen, int trglen, int src_max_phrase_len, int trg_max_phrase_len) :
      nodes(),
      edges(boost::extents[srclen][trglen][src_max_phrase_len+1][trg_max_phrase_len+1]),
      max_src_delta(boost::extents[srclen][trglen]),
      node_addresses(boost::extents[srclen][trglen]),
      valid_deltas(boost::extents[srclen][trglen]) {
    ComputeReachability(srclen, trglen, src_max_phrase_len, trg_max_phrase_len);
  }

 private:
  void ComputeReachability(int srclen, int trglen, int src_max_phrase_len, int trg_max_phrase_len);
};

#endif
