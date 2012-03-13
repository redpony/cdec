#ifndef _NS_COMB_H_
#define _NS_COMB_H_

#include "ns.h"

class CombinationMetric : public EvaluationMetric {
 public:
  CombinationMetric(const std::string& cmd);
  virtual boost::shared_ptr<SegmentEvaluator> CreateSegmentEvaluator(const std::vector<std::vector<WordID> >& refs) const;
  virtual float ComputeScore(const SufficientStats& stats) const;
  virtual unsigned SufficientStatisticsVectorSize() const;
 private:
  std::vector<EvaluationMetric*> metrics;
  std::vector<float> coeffs;
  std::vector<unsigned> offsets;
  unsigned total_size;
};

#endif
