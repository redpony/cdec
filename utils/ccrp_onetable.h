#ifndef _CCRP_ONETABLE_H_
#define _CCRP_ONETABLE_H_

#include <numeric>
#include <cassert>
#include <cmath>
#include <list>
#include <iostream>
#include <tr1/unordered_map>
#include <boost/functional/hash.hpp>
#include "sampler.h"
#include "slice_sampler.h"

// Chinese restaurant process (Pitman-Yor parameters) with one table approximation

template <typename Dish, typename DishHash = boost::hash<Dish> >
class CCRP_OneTable {
  typedef std::tr1::unordered_map<Dish, unsigned, DishHash> DishMapType;
 public:
  CCRP_OneTable(double disc, double conc) :
    num_tables_(),
    num_customers_(),
    discount_(disc),
    alpha_(conc),
    discount_prior_alpha_(std::numeric_limits<double>::quiet_NaN()),
    discount_prior_beta_(std::numeric_limits<double>::quiet_NaN()),
    alpha_prior_shape_(std::numeric_limits<double>::quiet_NaN()),
    alpha_prior_rate_(std::numeric_limits<double>::quiet_NaN()) {}

  CCRP_OneTable(double d_alpha, double d_beta, double c_shape, double c_rate, double d = 0.9, double c = 1.0) :
    num_tables_(),
    num_customers_(),
    discount_(d),
    alpha_(c),
    discount_prior_alpha_(d_alpha),
    discount_prior_beta_(d_beta),
    alpha_prior_shape_(c_shape),
    alpha_prior_rate_(c_rate) {}

  double discount() const { return discount_; }
  double alpha() const { return alpha_; }
  void set_alpha(double c) { alpha_ = c; }
  void set_discount(double d) { discount_ = d; }

  bool has_discount_prior() const {
    return !std::isnan(discount_prior_alpha_);
  }

  bool has_alpha_prior() const {
    return !std::isnan(alpha_prior_shape_);
  }

  void clear() {
    num_tables_ = 0;
    num_customers_ = 0;
    dish_counts_.clear();
  }

  unsigned num_tables() const {
    return num_tables_;
  }

  unsigned num_tables(const Dish& dish) const {
    const typename DishMapType::const_iterator it = dish_counts_.find(dish);
    if (it == dish_counts_.end()) return 0;
    return 1;
  }

  unsigned num_customers() const {
    return num_customers_;
  }

  unsigned num_customers(const Dish& dish) const {
    const typename DishMapType::const_iterator it = dish_counts_.find(dish);
    if (it == dish_counts_.end()) return 0;
    return it->second;
  }

  // returns +1 or 0 indicating whether a new table was opened
  int increment(const Dish& dish) {
    unsigned& dc = dish_counts_[dish];
    ++dc;
    ++num_customers_;
    if (dc == 1) {
      ++num_tables_;
      return 1;
    } else {
      return 0;
    }
  }

  // returns -1 or 0, indicating whether a table was closed
  int decrement(const Dish& dish) {
    unsigned& dc = dish_counts_[dish];
    assert(dc > 0);
    if (dc == 1) {
      dish_counts_.erase(dish);
      --num_tables_;
      --num_customers_;
      return -1;
    } else {
      assert(dc > 1);
      --dc;
      --num_customers_;
      return 0;
    }
  }

  double prob(const Dish& dish, const double& p0) const {
    const typename DishMapType::const_iterator it = dish_counts_.find(dish);
    const double r = num_tables_ * discount_ + alpha_;
    if (it == dish_counts_.end()) {
      return r * p0 / (num_customers_ + alpha_);
    } else {
      return (it->second - discount_ + r * p0) /
               (num_customers_ + alpha_);
    }
  }

  template <typename T>
  T probT(const Dish& dish, const T& p0) const {
    const typename DishMapType::const_iterator it = dish_counts_.find(dish);
    const T r(num_tables_ * discount_ + alpha_);
    if (it == dish_counts_.end()) {
      return r * p0 / T(num_customers_ + alpha_);
    } else {
      return (T(it->second - discount_) + r * p0) /
               T(num_customers_ + alpha_);
    }
  }

  double log_crp_prob() const {
    return log_crp_prob(discount_, alpha_);
  }

  static double log_beta_density(const double& x, const double& alpha, const double& beta) {
    assert(x > 0.0);
    assert(x < 1.0);
    assert(alpha > 0.0);
    assert(beta > 0.0);
    const double lp = (alpha-1)*log(x)+(beta-1)*log(1-x)+lgamma(alpha+beta)-lgamma(alpha)-lgamma(beta);
    return lp;
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
  double log_crp_prob(const double& discount, const double& alpha) const {
    double lp = 0.0;
    if (has_discount_prior())
      lp = log_beta_density(discount, discount_prior_alpha_, discount_prior_beta_);
    if (has_alpha_prior())
      lp += log_gamma_density(alpha, alpha_prior_shape_, alpha_prior_rate_);
    assert(lp <= 0.0);
    if (num_customers_) {
      if (discount > 0.0) {
        const double r = lgamma(1.0 - discount);
        lp += lgamma(alpha) - lgamma(alpha + num_customers_)
             + num_tables_ * log(discount) + lgamma(alpha / discount + num_tables_)
             - lgamma(alpha / discount);
        assert(std::isfinite(lp));
        for (typename DishMapType::const_iterator it = dish_counts_.begin();
             it != dish_counts_.end(); ++it) {
          const unsigned& cur = it->second;
          lp += lgamma(cur - discount) - r;
        }
      } else {
        assert(!"not implemented yet");
      }
    }
    assert(std::isfinite(lp));
    return lp;
  }

  void resample_hyperparameters(MT19937* rng, const unsigned nloop = 5, const unsigned niterations = 10) {
    assert(has_discount_prior() || has_alpha_prior());
    DiscountResampler dr(*this);
    ConcentrationResampler cr(*this);
    for (int iter = 0; iter < nloop; ++iter) {
      if (has_alpha_prior()) {
        alpha_ = slice_sampler1d(cr, alpha_, *rng, 0.0,
                               std::numeric_limits<double>::infinity(), 0.0, niterations, 100*niterations);
      }
      if (has_discount_prior()) {
        discount_ = slice_sampler1d(dr, discount_, *rng, std::numeric_limits<double>::min(),
                               1.0, 0.0, niterations, 100*niterations);
      }
    }
    alpha_ = slice_sampler1d(cr, alpha_, *rng, 0.0,
                             std::numeric_limits<double>::infinity(), 0.0, niterations, 100*niterations);
  }

  struct DiscountResampler {
    DiscountResampler(const CCRP_OneTable& crp) : crp_(crp) {}
    const CCRP_OneTable& crp_;
    double operator()(const double& proposed_discount) const {
      return crp_.log_crp_prob(proposed_discount, crp_.alpha_);
    }
  };

  struct ConcentrationResampler {
    ConcentrationResampler(const CCRP_OneTable& crp) : crp_(crp) {}
    const CCRP_OneTable& crp_;
    double operator()(const double& proposed_alpha) const {
      return crp_.log_crp_prob(crp_.discount_, proposed_alpha);
    }
  };

  void Print(std::ostream* out) const {
    (*out) << "PYP(d=" << discount_ << ",c=" << alpha_ << ") customers=" << num_customers_ << std::endl;
    for (typename DishMapType::const_iterator it = dish_counts_.begin(); it != dish_counts_.end(); ++it) {
      (*out) << "  " << it->first << " = " << it->second << std::endl;
    }
  }

  typedef typename DishMapType::const_iterator const_iterator;
  const_iterator begin() const {
    return dish_counts_.begin();
  }
  const_iterator end() const {
    return dish_counts_.end();
  }

  unsigned num_tables_;
  unsigned num_customers_;
  DishMapType dish_counts_;

  double discount_;
  double alpha_;

  // optional beta prior on discount_ (NaN if no prior)
  double discount_prior_alpha_;
  double discount_prior_beta_;

  // optional gamma prior on alpha_ (NaN if no prior)
  double alpha_prior_shape_;
  double alpha_prior_rate_;
};

template <typename T,typename H>
std::ostream& operator<<(std::ostream& o, const CCRP_OneTable<T,H>& c) {
  c.Print(&o);
  return o;
}

#endif
