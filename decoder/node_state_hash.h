#ifndef _NODE_STATE_HASH_
#define _NODE_STATE_HASH_

#include <cassert>
#include <cstring>
#include "tdict.h"
#include "murmur_hash3.h"
#include "ffset.h"

namespace cdec {

  struct FirstPassNode {
    FirstPassNode(int cat, int i, int j, int pi, int pj) : s(i), t(j), u(pi), v(pj) {
      memset(lhs, 0, 120);
      unsigned it = 0;
      for (auto& c : TD::Convert(-cat)) { lhs[it++] = c; if (it == 120) break; }
    }
    char lhs[120];
    short s;
    short t;
    short u;
    short v;
  };

  inline uint64_t HashNode(int cat, int i, int j, int pi, int pj) {
    FirstPassNode fpn(cat, i, j, pi, pj);
    return MurmurHash3_64(&fpn, sizeof(FirstPassNode), 2654435769U);
  }

  inline uint64_t HashNode(uint64_t old_hash, const FFState& state) {
    if (state.size() == 0) return old_hash;
    uint8_t buf[1024];
    std::memcpy(buf, &old_hash, sizeof(uint64_t));
    assert(state.size() < (1024u - sizeof(uint64_t)));
    std::memcpy(&buf[sizeof(uint64_t)], state.begin(), state.size());
    return MurmurHash3_64(buf, sizeof(uint64_t) + state.size(), 2654435769U);
  }

}

#endif

