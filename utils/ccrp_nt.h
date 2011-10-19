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

// Chinese restaurant process (1 parameter)
template <typename Dish, typename DishHash = boost::hash<Dish> >
class CCRP_NoTable {
 public:
  explicit CCRP_NoTable(double conc) :
    num_customers_(),
    concentration_(conc),
    concentration_prior_shape_(std::numeric_limits<double>::quiet_NaN()),
    concentration_prior_rate_(std::numeric_limits<double>::quiet_NaN()) {}

  CCRP_NoTable(double c_shape, double c_rate, double c = 10.0) :
    num_customers_(),
    concentration_(c),
    concentration_prior_shape_(c_shape),
    concentration_prior_rate_(c_rate) {}

  double concentration() const { return concentration_; }

  bool has_concentration_prior() const {
    return !std::isnan(concentration_prior_shape_);
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

  double prob(const Dish& dish, const double& p0) const {
    const unsigned at_table = num_customers(dish);
    return (at_table + p0 * concentration_) / (num_customers_ + concentration_);
  }

  double logprob(const Dish& dish, const double& logp0) const {
    const unsigned at_table = num_customers(dish);
    return log(at_table + exp(logp0 + log(concentration_))) - log(num_customers_ + concentration_);
  }

  double log_crp_prob() const {
    return log_crp_prob(concentration_);
  }

  static double log_gamma_density(const double& x, const double& shape, const double& rate) {
    assert(x >= 0.0);
    assert(shape > 0.0);
    assert(rate > 0.0);
    const double lp = (shape-1)*log(x) - shape*log(rate) - x/rate - lgamma(shape);
    return lp;
  }

  // taken from http://en.wikipedia.org/wiki/Chinese_restaurant_process
  // does not include P_0's
  double log_crp_prob(const double& concentration) const {
    double lp = 0.0;
    if (has_concentration_prior())
      lp += log_gamma_density(concentration, concentration_prior_shape_, concentration_prior_rate_);
    assert(lp <= 0.0);
    if (num_customers_) {
      lp += lgamma(concentration) - lgamma(concentration + num_customers_) +
        custs_.size() * log(concentration);
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
    assert(has_concentration_prior());
    ConcentrationResampler cr(*this);
    for (int iter = 0; iter < nloop; ++iter) {
        concentration_ = slice_sampler1d(cr, concentration_, *rng, 0.0,
                               std::numeric_limits<double>::infinity(), 0.0, niterations, 100*niterations);
    }
  }

  struct ConcentrationResampler {
    ConcentrationResampler(const CCRP_NoTable& crp) : crp_(crp) {}
    const CCRP_NoTable& crp_;
    double operator()(const double& proposed_concentration) const {
      return crp_.log_crp_prob(proposed_concentration);
    }
  };

  void Print(std::ostream* out) const {
    (*out) << "DP(alpha=" << concentration_ << ") customers=" << num_customers_ << std::endl;
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

  double concentration_;

  // optional gamma prior on concentration_ (NaN if no prior)
  double concentration_prior_shape_;
  double concentration_prior_rate_;
};

template <typename T,typename H>
std::ostream& operator<<(std::ostream& o, const CCRP_NoTable<T,H>& c) {
  c.Print(&o);
  return o;
}

#endif
