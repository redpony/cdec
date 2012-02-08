#ifndef _CONDITIONAL_PSEG_H_
#define _CONDITIONAL_PSEG_H_

#include <vector>
#include <tr1/unordered_map>
#include <boost/functional/hash.hpp>
#include <iostream>

#include "m.h"
#include "prob.h"
#include "ccrp_nt.h"
#include "mfcr.h"
#include "trule.h"
#include "base_distributions.h"
#include "tdict.h"

template <typename ConditionalBaseMeasure>
struct MConditionalTranslationModel {
  explicit MConditionalTranslationModel(ConditionalBaseMeasure& rcp0) :
    rp0(rcp0), lambdas(1, 1.0), p0s(1) {}

  void Summary() const {
    std::cerr << "Number of conditioning contexts: " << r.size() << std::endl;
    for (RuleModelHash::const_iterator it = r.begin(); it != r.end(); ++it) {
      std::cerr << TD::GetString(it->first) << "   \t(d=" << it->second.d() << ",\\alpha = " << it->second.alpha() << ") --------------------------" << std::endl;
      for (MFCR<TRule>::const_iterator i2 = it->second.begin(); i2 != it->second.end(); ++i2)
        std::cerr << "   " << -1 << '\t' << i2->first << std::endl;
    }
  }

  void ResampleHyperparameters(MT19937* rng) {
    for (RuleModelHash::iterator it = r.begin(); it != r.end(); ++it)
      it->second.resample_hyperparameters(rng);
  } 

  int DecrementRule(const TRule& rule, MT19937* rng) {
    RuleModelHash::iterator it = r.find(rule.f_);
    assert(it != r.end());
    const TableCount delta = it->second.decrement(rule, rng);
    if (delta.count) {
      if (it->second.num_customers() == 0) r.erase(it);
    }
    return delta.count;
  }

  int IncrementRule(const TRule& rule, MT19937* rng) {
    RuleModelHash::iterator it = r.find(rule.f_);
    if (it == r.end()) {
      it = r.insert(make_pair(rule.f_, MFCR<TRule>(1, 1.0, 1.0, 1.0, 1.0, 1e-9, 4.0))).first;
    }
    p0s[0] = rp0(rule).as_float(); 
    TableCount delta = it->second.increment(rule, p0s, lambdas, rng);
    return delta.count;
  }

  prob_t RuleProbability(const TRule& rule) const {
    prob_t p;
    RuleModelHash::const_iterator it = r.find(rule.f_);
    if (it == r.end()) {
      p.logeq(log(rp0(rule)));
    } else {
      p0s[0] = rp0(rule).as_float();
      p = prob_t(it->second.prob(rule, p0s, lambdas));
    }
    return p;
  }

  prob_t Likelihood() const {
    prob_t p = prob_t::One();
#if 0
    for (RuleModelHash::const_iterator it = r.begin(); it != r.end(); ++it) {
      prob_t q; q.logeq(it->second.log_crp_prob());
      p *= q;
      for (CCRP_NoTable<TRule>::const_iterator i2 = it->second.begin(); i2 != it->second.end(); ++i2)
        p *= rp0(i2->first);
    }
#endif
    return p;
  }

  const ConditionalBaseMeasure& rp0;
  typedef std::tr1::unordered_map<std::vector<WordID>,
                                  MFCR<TRule>,
                                  boost::hash<std::vector<WordID> > > RuleModelHash;
  RuleModelHash r;
  std::vector<double> lambdas;
  mutable std::vector<double> p0s;
};

template <typename ConditionalBaseMeasure>
struct ConditionalTranslationModel {
  explicit ConditionalTranslationModel(ConditionalBaseMeasure& rcp0) :
    rp0(rcp0) {}

  void Summary() const {
    std::cerr << "Number of conditioning contexts: " << r.size() << std::endl;
    for (RuleModelHash::const_iterator it = r.begin(); it != r.end(); ++it) {
      std::cerr << TD::GetString(it->first) << "   \t(\\alpha = " << it->second.concentration() << ") --------------------------" << std::endl;
      for (CCRP_NoTable<TRule>::const_iterator i2 = it->second.begin(); i2 != it->second.end(); ++i2)
        std::cerr << "   " << i2->second << '\t' << i2->first << std::endl;
    }
  }

  void ResampleHyperparameters(MT19937* rng) {
    for (RuleModelHash::iterator it = r.begin(); it != r.end(); ++it)
      it->second.resample_hyperparameters(rng);
  } 

  int DecrementRule(const TRule& rule) {
    RuleModelHash::iterator it = r.find(rule.f_);
    assert(it != r.end());    
    int count = it->second.decrement(rule);
    if (count) {
      if (it->second.num_customers() == 0) r.erase(it);
    }
    return count;
  }

  int IncrementRule(const TRule& rule) {
    RuleModelHash::iterator it = r.find(rule.f_);
    if (it == r.end()) {
      it = r.insert(make_pair(rule.f_, CCRP_NoTable<TRule>(1.0, 1.0, 8.0))).first;
    } 
    int count = it->second.increment(rule);
    return count;
  }

  void IncrementRules(const std::vector<TRulePtr>& rules) {
    for (int i = 0; i < rules.size(); ++i)
      IncrementRule(*rules[i]);
  }

  void DecrementRules(const std::vector<TRulePtr>& rules) {
    for (int i = 0; i < rules.size(); ++i)
      DecrementRule(*rules[i]);
  }

  prob_t RuleProbability(const TRule& rule) const {
    prob_t p;
    RuleModelHash::const_iterator it = r.find(rule.f_);
    if (it == r.end()) {
      p.logeq(log(rp0(rule)));
    } else {
      p.logeq(it->second.logprob(rule, log(rp0(rule))));
    }
    return p;
  }

  prob_t Likelihood() const {
    prob_t p = prob_t::One();
    for (RuleModelHash::const_iterator it = r.begin(); it != r.end(); ++it) {
      prob_t q; q.logeq(it->second.log_crp_prob());
      p *= q;
      for (CCRP_NoTable<TRule>::const_iterator i2 = it->second.begin(); i2 != it->second.end(); ++i2)
        p *= rp0(i2->first);
    }
    return p;
  }

  const ConditionalBaseMeasure& rp0;
  typedef std::tr1::unordered_map<std::vector<WordID>,
                                  CCRP_NoTable<TRule>,
                                  boost::hash<std::vector<WordID> > > RuleModelHash;
  RuleModelHash r;
};

template <typename ConditionalBaseMeasure>
struct ConditionalParallelSegementationModel {
  explicit ConditionalParallelSegementationModel(ConditionalBaseMeasure& rcp0) :
    tmodel(rcp0), base(prob_t::One()), aligns(1,1) {}

  ConditionalTranslationModel<ConditionalBaseMeasure> tmodel;

  void DecrementRule(const TRule& rule) {
    tmodel.DecrementRule(rule);
  }

  void IncrementRule(const TRule& rule) {
    tmodel.IncrementRule(rule);
  }

  void IncrementRulesAndAlignments(const std::vector<TRulePtr>& rules) {
    tmodel.IncrementRules(rules);
    for (int i = 0; i < rules.size(); ++i) {
      IncrementAlign(rules[i]->f_.size());
    }
  }

  void DecrementRulesAndAlignments(const std::vector<TRulePtr>& rules) {
    tmodel.DecrementRules(rules);
    for (int i = 0; i < rules.size(); ++i) {
      DecrementAlign(rules[i]->f_.size());
    }
  }

  prob_t RuleProbability(const TRule& rule) const {
    return tmodel.RuleProbability(rule);
  }

  void IncrementAlign(unsigned span) {
    if (aligns.increment(span)) {
      // TODO
    }
  }

  void DecrementAlign(unsigned span) {
    if (aligns.decrement(span)) {
      // TODO
    }
  }

  prob_t AlignProbability(unsigned span) const {
    prob_t p;
    p.logeq(aligns.logprob(span, Md::log_poisson(span, 1.0)));
    return p;
  }

  prob_t Likelihood() const {
    prob_t p; p.logeq(aligns.log_crp_prob());
    p *= base;
    p *= tmodel.Likelihood();
    return p;
  }

  prob_t base;
  CCRP_NoTable<unsigned> aligns;
};

#endif

