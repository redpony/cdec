#ifndef NS_CER_H_
#define NS_CER_H_

#include "ns.h"

class CERMetric : public EvaluationMetric {
  friend class EvaluationMetric;
 protected:
  CERMetric() : EvaluationMetric("CER") {}

 public:
  virtual bool IsErrorMetric() const;
  virtual unsigned SufficientStatisticsVectorSize() const;
  virtual void ComputeSufficientStatistics(const std::vector<WordID>& hyp,
                                           const std::vector<std::vector<WordID> >& refs,
                                           SufficientStats* out) const;
  virtual float ComputeScore(const SufficientStats& stats) const;
};

#endif
