#ifndef _NS_WER_H_
#define _NS_WER_H_

#include "ns.h"

class WERMetric : public EvaluationMetric {
  friend class EvaluationMetric;
 protected:
  WERMetric() : EvaluationMetric("WER") {}

 public:
  virtual bool IsErrorMetric() const;
  virtual unsigned SufficientStatisticsVectorSize() const;
  virtual void ComputeSufficientStatistics(const std::vector<WordID>& hyp,
                                           const std::vector<std::vector<WordID> >& refs,
                                           SufficientStats* out) const;
  virtual float ComputeScore(const SufficientStats& stats) const;
};

#endif
