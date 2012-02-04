#ifndef _MONOTONIC_PSEG_H_
#define _MONOTONIC_PSEG_H_

#include <vector>

#include "prob.h"
#include "ccrp_nt.h"
#include "trule.h"
#include "base_distributions.h"

template <typename BaseMeasure>
struct MonotonicParallelSegementationModel {
  explicit MonotonicParallelSegementationModel(BaseMeasure& rcp0) :
    rp0(rcp0), base(prob_t::One()), rules(1,1), stop(1.0) {}

  void DecrementRule(const TRule& rule) {
    if (rules.decrement(rule))
      base /= rp0(rule);
  }

  void IncrementRule(const TRule& rule) {
    if (rules.increment(rule))
      base *= rp0(rule);
  }

  void IncrementRulesAndStops(const std::vector<TRulePtr>& rules) {
    for (int i = 0; i < rules.size(); ++i)
      IncrementRule(*rules[i]);
    if (rules.size()) IncrementContinue(rules.size() - 1);
    IncrementStop();
  }

  void DecrementRulesAndStops(const std::vector<TRulePtr>& rules) {
    for (int i = 0; i < rules.size(); ++i)
      DecrementRule(*rules[i]);
    if (rules.size()) {
      DecrementContinue(rules.size() - 1);
      DecrementStop();
    }
  }

  prob_t RuleProbability(const TRule& rule) const {
    prob_t p; p.logeq(rules.logprob(rule, log(rp0(rule))));
    return p;
  }

  prob_t Likelihood() const {
    prob_t p = base;
    prob_t q; q.logeq(rules.log_crp_prob());
    p *= q;
    q.logeq(stop.log_crp_prob());
    p *= q;
    return p;
  }

  void IncrementStop() {
    stop.increment(true);
  }

  void IncrementContinue(int n = 1) {
    for (int i = 0; i < n; ++i)
      stop.increment(false);
  }

  void DecrementStop() {
    stop.decrement(true);
  }

  void DecrementContinue(int n = 1) {
    for (int i = 0; i < n; ++i)
      stop.decrement(false);
  }

  prob_t StopProbability() const {
    return prob_t(stop.prob(true, 0.5));
  }

  prob_t ContinueProbability() const {
    return prob_t(stop.prob(false, 0.5));
  }

  const BaseMeasure& rp0;
  prob_t base;
  CCRP_NoTable<TRule> rules;
  CCRP_NoTable<bool> stop;
};

#endif

