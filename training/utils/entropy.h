#ifndef _CSENTROPY_H_
#define _CSENTROPY_H_

#include <vector>
#include "sparse_vector.h"

namespace training {
  class CandidateSet;

  class CandidateSetEntropy {
   public:
    explicit CandidateSetEntropy(const CandidateSet& cs) : cands_(cs) {}
    // compute the entropy (expected log likelihood) of a CandidateSet
    // (optional) the gradient of the entropy with respect to params
    double operator()(const std::vector<double>& params,
                      SparseVector<double>* g = NULL) const;
   private:
    const CandidateSet& cands_;
  };
};

#endif
