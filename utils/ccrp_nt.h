#ifndef _CCRP_NT_H_
#define _CCRP_NT_H_

#include <numeric>
#include <cassert>
#include <cmath>
#include <list>
#include <iostream>
#include <vector>
#include <tr1/unordered_map>
#include <boost/functional/hash.hpp>
#include "sampler.h"
#include "slice_sampler.h"
#include "m.h"

// Chinese restaurant process (1 parameter)
template <typename Dish, typename DishHash = boost::hash<Dish> >
class CCRP_NoTable {
 public:
  explicit CCRP_NoTable(double conc) :
    num_customers_(),
    alpha_(conc),
    alpha_prior_shape_(std::numeric_limits<double>::quiet_NaN()),
    alpha_prior_rate_(std::numeric_limits<double>::quiet_NaN()) {}

  CCRP_NoTable(double c_shape, double c_rate, double c = 10.0) :
    num_customers_(),
    alpha_(c),
    alpha_prior_shape_(c_shape),
    alpha_prior_rate_(c_rate) {}

  double alpha() const { return alpha_; }
  void set_alpha(const double& alpha) { alpha_ = alpha; assert(alpha_ > 0.0); }

  bool has_alpha_prior() const {
    return !std::isnan(alpha_prior_shape_);
  }

  void clear() {
    num_customers_ = 0;
    custs_.clear();
  }

  unsigned num_customers() const {
    return num_customers_;
  }

  unsigned num_customers(const Dish& dish) const {
    const typename std::tr1::unordered_map<Dish, unsigned, DishHash>::const_iterator it = custs_.find(dish);
    if (it == custs_.end()) return 0;
    return it->second;
  }

  int increment(const Dish& dish) {
    int table_diff = 0;
    if (++custs_[dish] == 1)
      table_diff = 1;
    ++num_customers_;
    return table_diff;
  }

  int decrement(const Dish& dish) {
    int table_diff = 0;
    int nc = --custs_[dish];
    if (nc == 0) {
      custs_.erase(dish);
      table_diff = -1;
    } else if (nc < 0) {
      std::cerr << "Dish counts dropped below zero for: " << dish << std::endl;
      abort();
    }
    --num_customers_;
    return table_diff;
  }

  template <typename F>
  F prob(const Dish& dish, const F& p0) const {
    const unsigned at_table = num_customers(dish);
    return (F(at_table) + p0 * F(alpha_)) / F(num_customers_ + alpha_);
  }

  double logprob(const Dish& dish, const double& logp0) const {
    const unsigned at_table = num_customers(dish);
    return log(at_table + exp(logp0 + log(alpha_))) - log(num_customers_ + alpha_);
  }

  double log_crp_prob() const {
    return log_crp_prob(alpha_);
  }

  // taken from http://en.wikipedia.org/wiki/Chinese_restaurant_process
  // does not include P_0's
  double log_crp_prob(const double& alpha) const {
    double lp = 0.0;
    if (has_alpha_prior())
      lp += Md::log_gamma_density(alpha, alpha_prior_shape_, alpha_prior_rate_);
    assert(lp <= 0.0);
    if (num_customers_) {
      lp += lgamma(alpha) - lgamma(alpha + num_customers_) +
        custs_.size() * log(alpha);
      assert(std::isfinite(lp));
      for (typename std::tr1::unordered_map<Dish, unsigned, DishHash>::const_iterator it = custs_.begin();
             it != custs_.end(); ++it) {
          lp += lgamma(it->second);
      }
    }
    assert(std::isfinite(lp));
    return lp;
  }

  void resample_hyperparameters(MT19937* rng, const unsigned nloop = 5, const unsigned niterations = 10) {
    assert(has_alpha_prior());
    ConcentrationResampler cr(*this);
    for (unsigned iter = 0; iter < nloop; ++iter) {
        alpha_ = slice_sampler1d(cr, alpha_, *rng, 0.0,
                               std::numeric_limits<double>::infinity(), 0.0, niterations, 100*niterations);
    }
  }

  struct ConcentrationResampler {
    ConcentrationResampler(const CCRP_NoTable& crp) : crp_(crp) {}
    const CCRP_NoTable& crp_;
    double operator()(const double& proposed_alpha) const {
      return crp_.log_crp_prob(proposed_alpha);
    }
  };

  void Print(std::ostream* out) const {
    (*out) << "DP(alpha=" << alpha_ << ") customers=" << num_customers_ << std::endl;
    int cc = 0;
    for (typename std::tr1::unordered_map<Dish, unsigned, DishHash>::const_iterator it = custs_.begin();
         it != custs_.end(); ++it) {
      (*out) << " " << it->first << "(" << it->second << " eating)";
      ++cc;
      if (cc > 10) { (*out) << " ..."; break; }
    }
    (*out) << std::endl;
  }

  unsigned num_customers_;
  std::tr1::unordered_map<Dish, unsigned, DishHash> custs_;

  typedef typename std::tr1::unordered_map<Dish, unsigned, DishHash>::const_iterator const_iterator;
  const_iterator begin() const {
    return custs_.begin();
  }
  const_iterator end() const {
    return custs_.end();
  }

  double alpha_;

  // optional gamma prior on alpha_ (NaN if no prior)
  double alpha_prior_shape_;
  double alpha_prior_rate_;
};

template <typename T,typename H>
std::ostream& operator<<(std::ostream& o, const CCRP_NoTable<T,H>& c) {
  c.Print(&o);
  return o;
}

#endif
