#ifndef _NS_CER_H_
#define _NS_CER_H_

#include "ns.h"

class CERMetric : public EvaluationMetric {
  friend class EvaluationMetric;
 private:
  unsigned EditDistance(const std::string& hyp,
                        const std::string& ref) const;
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
