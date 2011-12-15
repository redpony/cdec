#ifndef _NS_TER_H_
#define _NS_TER_H_

#include "ns.h"

class TERMetric : public EvaluationMetric {
  friend class EvaluationMetric;
 protected:
  TERMetric() : EvaluationMetric("TER") {}

 public:
  virtual void ComputeSufficientStatistics(const std::vector<WordID>& hyp,
                                           const std::vector<std::vector<WordID> >& refs,
                                           SufficientStats* out) const;
  virtual float ComputeScore(const SufficientStats& stats) const;
};

#endif
