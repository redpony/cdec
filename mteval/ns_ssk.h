#ifndef _NS_SSK_H_
#define _NS_SSK_H_

#include "ns.h"

class SSKMetric : public EvaluationMetric {
  friend class EvaluationMetric;
 private:
  unsigned EditDistance(const std::string& hyp,
                        const std::string& ref) const;
 protected:
  SSKMetric() : EvaluationMetric("SSK") {}

 public:
  virtual unsigned SufficientStatisticsVectorSize() const;
  virtual void ComputeSufficientStatistics(const std::vector<WordID>& hyp,
                                           const std::vector<std::vector<WordID> >& refs,
                                           SufficientStats* out) const;
  virtual float ComputeScore(const SufficientStats& stats) const;
};

#endif
