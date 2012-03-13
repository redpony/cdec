#include "reachability.h"

#include <vector>
#include <iostream>

using namespace std;

struct SState {
  SState() : prev_src_covered(), prev_trg_covered() {}
  SState(int i, int j) : prev_src_covered(i), prev_trg_covered(j) {}
  int prev_src_covered;
  int prev_trg_covered;
};

void Reachability::ComputeReachability(int srclen, int trglen, int src_max_phrase_len, int trg_max_phrase_len) {
    typedef boost::multi_array<vector<SState>, 2> array_type;
    array_type a(boost::extents[srclen + 1][trglen + 1]);
    a[0][0].push_back(SState());
    for (int i = 0; i < srclen; ++i) {
      for (int j = 0; j < trglen; ++j) {
        if (a[i][j].size() == 0) continue;
        const SState prev(i,j);
        for (int k = 1; k <= src_max_phrase_len; ++k) {
          if ((i + k) > srclen) continue;
          for (int l = 1; l <= trg_max_phrase_len; ++l) {
            if ((j + l) > trglen) continue;
            a[i + k][j + l].push_back(prev);
          }
        }
      }
    }
    a[0][0].clear();
    //cerr << srclen << "," << trglen << ": Final cell contains " << a[srclen][trglen].size() << " back pointers\n";
    if (a[srclen][trglen].empty()) {
      cerr << "Sequence pair with lengths (" << srclen << ',' << trglen << ") violates reachability constraints\n";
      nodes = 0;
      return;
    }

    typedef boost::multi_array<bool, 2> rarray_type;
    rarray_type r(boost::extents[srclen + 1][trglen + 1]);
    r[srclen][trglen] = true;
    nodes = 0;
    for (int i = srclen; i >= 0; --i) {
      for (int j = trglen; j >= 0; --j) {
        vector<SState>& prevs = a[i][j];
        if (!r[i][j]) { prevs.clear(); }
        for (int k = 0; k < prevs.size(); ++k) {
          r[prevs[k].prev_src_covered][prevs[k].prev_trg_covered] = true;
          int src_delta = i - prevs[k].prev_src_covered;
          edges[prevs[k].prev_src_covered][prevs[k].prev_trg_covered][src_delta][j - prevs[k].prev_trg_covered] = true;
          valid_deltas[prevs[k].prev_src_covered][prevs[k].prev_trg_covered].push_back(make_pair<short,short>(src_delta,j - prevs[k].prev_trg_covered));
          short &msd = max_src_delta[prevs[k].prev_src_covered][prevs[k].prev_trg_covered];
          if (src_delta > msd) msd = src_delta;
        }
      }
    }
    assert(!edges[0][0][1][0]);
    assert(!edges[0][0][0][1]);
    assert(!edges[0][0][0][0]);
    assert(max_src_delta[0][0] > 0);
    nodes = 0;
    for (int i = 0; i < srclen; ++i) {
      for (int j = 0; j < trglen; ++j) {
        if (valid_deltas[i][j].size() > 0) {
          node_addresses[i][j] = nodes++;
        } else {
          node_addresses[i][j] = -1;
        }
      }
    }
    cerr << "Sequence pair with lengths (" << srclen << ',' << trglen << ") has " << valid_deltas[0][0].size() << " out edges in its root node, " << nodes << " nodes in total, and outside estimate matrix will require " << sizeof(float)*nodes << " bytes\n";
  }

