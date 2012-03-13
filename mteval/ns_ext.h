#ifndef _NS_EXTERNAL_SCORER_H_
#define _NS_EXTERNAL_SCORER_H_

#include "ns.h"

struct NScoreServer;
class ExternalMetric : public EvaluationMetric {
 public:
  ExternalMetric(const std::string& metricid, const std::string& command);
  ~ExternalMetric();

  virtual void ComputeSufficientStatistics(const std::vector<WordID>& hyp,
                                           const std::vector<std::vector<WordID> >& refs,
                                           SufficientStats* out) const;
  virtual float ComputeScore(const SufficientStats& stats) const;

 protected:
  NScoreServer* eval_server;
};

#endif
