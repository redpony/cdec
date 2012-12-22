#ifndef _RISK_H_
#define _RISK_H_

#include <vector>
#include "sparse_vector.h"
class EvaluationMetric;

namespace training {
  class CandidateSet;

  class CandidateSetRisk {
   public:
    explicit CandidateSetRisk(const CandidateSet& cs, const EvaluationMetric& metric) :
       cands_(cs),
       metric_(metric) {}
    // compute the risk (expected loss) of a CandidateSet
    // (optional) the gradient of the risk with respect to params
    double operator()(const std::vector<double>& params,
                      SparseVector<double>* g = NULL) const;
   private:
    const CandidateSet& cands_;
    const EvaluationMetric& metric_;
  };
};

#endif
