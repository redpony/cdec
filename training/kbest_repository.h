#ifndef _KBEST_REPOSITORY_H_
#define _KBEST_REPOSITORY_H_

#include <vector>
#include "wordid.h"
#include "ns.h"
#include "sparse_vector.h"

class KBestRepository {
  struct HypInfo {
    std::vector<WordID> words;
    SparseVector<double> x;
    SufficientStats score_stats;
  };

  std::vector<HypInfo> candidates;
};

#endif
