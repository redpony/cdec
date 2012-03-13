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
    rp0(rcp0), d(0.5), strength(1.0), lambdas(1, prob_t::One()), p0s(1) {}

  void Summary() const {
    std::cerr << "Number of conditioning contexts: " << r.size() << std::endl;
    for (RuleModelHash::const_iterator it = r.begin(); it != r.end(); ++it) {
      std::cerr << TD::GetString(it->first) << "   \t(d=" << it->second.discount() << ",s=" << it->second.strength() << ") --------------------------" << std::endl;
      for (MFCR<1,TRule>::const_iterator i2 = it->second.begin(); i2 != it->second.end(); ++i2)
        std::cerr << "   " << i2->second.total_dish_count_ << '\t' << i2->first << std::endl;
    }
  }

  double log_likelihood(const double& dd, const double& aa) const {
    if (aa <= -dd) return -std::numeric_limits<double>::infinity();
    //double llh = Md::log_beta_density(dd, 10, 3) + Md::log_gamma_density(aa, 1, 1);
    double llh = Md::log_beta_density(dd, 1, 1) +
                 Md::log_gamma_density(dd + aa, 1, 1);
    typename std::tr1::unordered_map<std::vector<WordID>, MFCR<1,TRule>, boost::hash<std::vector<WordID> > >::const_iterator it;
    for (it = r.begin(); it != r.end(); ++it)
      llh += it->second.log_crp_prob(dd, aa);
    return llh;
  }

  struct DiscountResampler {
    DiscountResampler(const MConditionalTranslationModel& m) : m_(m) {}
    const MConditionalTranslationModel& m_;
    double operator()(const double& proposed_discount) const {
      return m_.log_likelihood(proposed_discount, m_.strength);
    }
  };

  struct AlphaResampler {
    AlphaResampler(const MConditionalTranslationModel& m) : m_(m) {}
    const MConditionalTranslationModel& m_;
    double operator()(const double& proposed_strength) const {
      return m_.log_likelihood(m_.d, proposed_strength);
    }
  };

  void ResampleHyperparameters(MT19937* rng) {
    typename std::tr1::unordered_map<std::vector<WordID>, MFCR<1,TRule>, boost::hash<std::vector<WordID> > >::iterator it;
#if 1
    for (it = r.begin(); it != r.end(); ++it) {
      it->second.resample_hyperparameters(rng);
    }
#else
    const unsigned nloop = 5;
    const unsigned niterations = 10;
    DiscountResampler dr(*this);
    AlphaResampler ar(*this);
    for (int iter = 0; iter < nloop; ++iter) {
      strength = slice_sampler1d(ar, strength, *rng, -d + std::numeric_limits<double>::min(),
                              std::numeric_limits<double>::infinity(), 0.0, niterations, 100*niterations);
      double min_discount = std::numeric_limits<double>::min();
      if (strength < 0.0) min_discount -= strength;
      d = slice_sampler1d(dr, d, *rng, min_discount,
                          1.0, 0.0, niterations, 100*niterations);
    }
    strength = slice_sampler1d(ar, strength, *rng, -d,
                            std::numeric_limits<double>::infinity(), 0.0, niterations, 100*niterations);
    std::cerr << "MConditionalTranslationModel(d=" << d << ",s=" << strength << ") = " << log_likelihood(d, strength) << std::endl;
    for (it = r.begin(); it != r.end(); ++it) {
      it->second.set_discount(d);
      it->second.set_strength(strength);
    }
#endif
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
      //it = r.insert(make_pair(rule.f_, MFCR<1,TRule>(d, strength))).first;
      it = r.insert(make_pair(rule.f_, MFCR<1,TRule>(1,1,1,1,0.6, -0.12))).first;
    }
    p0s[0] = rp0(rule); 
    TableCount delta = it->second.increment(rule, p0s.begin(), lambdas.begin(), rng);
    return delta.count;
  }

  prob_t RuleProbability(const TRule& rule) const {
    prob_t p;
    RuleModelHash::const_iterator it = r.find(rule.f_);
    if (it == r.end()) {
      p = rp0(rule);
    } else {
      p0s[0] = rp0(rule);
      p = it->second.prob(rule, p0s.begin(), lambdas.begin());
    }
    return p;
  }

  prob_t Likelihood() const {
    prob_t p; p.logeq(log_likelihood(d, strength));
    return p;
  }

  const ConditionalBaseMeasure& rp0;
  typedef std::tr1::unordered_map<std::vector<WordID>,
                                  MFCR<1, TRule>,
                                  boost::hash<std::vector<WordID> > > RuleModelHash;
  RuleModelHash r;
  double d, strength;
  std::vector<prob_t> lambdas;
  mutable std::vector<prob_t> p0s;
};

template <typename ConditionalBaseMeasure>
struct ConditionalTranslationModel {
  explicit ConditionalTranslationModel(ConditionalBaseMeasure& rcp0) :
    rp0(rcp0) {}

  void Summary() const {
    std::cerr << "Number of conditioning contexts: " << r.size() << std::endl;
    for (RuleModelHash::const_iterator it = r.begin(); it != r.end(); ++it) {
      std::cerr << TD::GetString(it->first) << "   \t(\\alpha = " << it->second.alpha() << ") --------------------------" << std::endl;
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

