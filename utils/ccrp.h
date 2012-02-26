#ifndef _CCRP_H_
#define _CCRP_H_

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

// Chinese restaurant process (Pitman-Yor parameters) with table tracking.

template <typename Dish, typename DishHash = boost::hash<Dish> >
class CCRP {
 public:
  CCRP(double disc, double conc) :
    num_tables_(),
    num_customers_(),
    discount_(disc),
    concentration_(conc),
    discount_prior_alpha_(std::numeric_limits<double>::quiet_NaN()),
    discount_prior_beta_(std::numeric_limits<double>::quiet_NaN()),
    concentration_prior_shape_(std::numeric_limits<double>::quiet_NaN()),
    concentration_prior_rate_(std::numeric_limits<double>::quiet_NaN()) {}

  CCRP(double d_alpha, double d_beta, double c_shape, double c_rate, double d = 0.9, double c = 1.0) :
    num_tables_(),
    num_customers_(),
    discount_(d),
    concentration_(c),
    discount_prior_alpha_(d_alpha),
    discount_prior_beta_(d_beta),
    concentration_prior_shape_(c_shape),
    concentration_prior_rate_(c_rate) {}

  double discount() const { return discount_; }
  double concentration() const { return concentration_; }

  bool has_discount_prior() const {
    return !std::isnan(discount_prior_alpha_);
  }

  bool has_concentration_prior() const {
    return !std::isnan(concentration_prior_shape_);
  }

  void clear() {
    num_tables_ = 0;
    num_customers_ = 0;
    dish_locs_.clear();
  }

  unsigned num_tables() const {
    return num_tables_;
  }

  unsigned num_tables(const Dish& dish) const {
    const typename std::tr1::unordered_map<Dish, DishLocations, DishHash>::const_iterator it = dish_locs_.find(dish);
    if (it == dish_locs_.end()) return 0;
    return it->second.table_counts_.size();
  }

  unsigned num_customers() const {
    return num_customers_;
  }

  unsigned num_customers(const Dish& dish) const {
    const typename std::tr1::unordered_map<Dish, DishLocations, DishHash>::const_iterator it = dish_locs_.find(dish);
    if (it == dish_locs_.end()) return 0;
    return it->total_dish_count_;
  }

  // returns +1 or 0 indicating whether a new table was opened
  int increment(const Dish& dish, const double& p0, MT19937* rng) {
    DishLocations& loc = dish_locs_[dish];
    bool share_table = false;
    if (loc.total_dish_count_) {
      const double p_empty = (concentration_ + num_tables_ * discount_) * p0;
      const double p_share = (loc.total_dish_count_ - loc.table_counts_.size() * discount_);
      share_table = rng->SelectSample(p_empty, p_share);
    }
    if (share_table) {
      double r = rng->next() * (loc.total_dish_count_ - loc.table_counts_.size() * discount_);
      for (typename std::list<unsigned>::iterator ti = loc.table_counts_.begin();
           ti != loc.table_counts_.end(); ++ti) {
        r -= (*ti - discount_);
        if (r <= 0.0) {
          ++(*ti);
          break;
        }
      }
      if (r > 0.0) {
        std::cerr << "Serious error: r=" << r << std::endl;
        Print(&std::cerr);
        assert(r <= 0.0);
      }
    } else {
      loc.table_counts_.push_back(1u);
      ++num_tables_;
    }
    ++loc.total_dish_count_;
    ++num_customers_;
    return (share_table ? 0 : 1);
  }

  // returns +1 or 0 indicating whether a new table was opened
  template <typename T>
  int incrementT(const Dish& dish, const T& p0, MT19937* rng) {
    DishLocations& loc = dish_locs_[dish];
    bool share_table = false;
    if (loc.total_dish_count_) {
      const T p_empty = T(concentration_ + num_tables_ * discount_) * p0;
      const T p_share = T(loc.total_dish_count_ - loc.table_counts_.size() * discount_);
      share_table = rng->SelectSample(p_empty, p_share);
    }
    if (share_table) {
      double r = rng->next() * (loc.total_dish_count_ - loc.table_counts_.size() * discount_);
      for (typename std::list<unsigned>::iterator ti = loc.table_counts_.begin();
           ti != loc.table_counts_.end(); ++ti) {
        r -= (*ti - discount_);
        if (r <= 0.0) {
          ++(*ti);
          break;
        }
      }
      if (r > 0.0) {
        std::cerr << "Serious error: r=" << r << std::endl;
        Print(&std::cerr);
        assert(r <= 0.0);
      }
    } else {
      loc.table_counts_.push_back(1u);
      ++num_tables_;
    }
    ++loc.total_dish_count_;
    ++num_customers_;
    return (share_table ? 0 : 1);
  }

  // returns -1 or 0, indicating whether a table was closed
  int decrement(const Dish& dish, MT19937* rng) {
    DishLocations& loc = dish_locs_[dish];
    assert(loc.total_dish_count_);
    if (loc.total_dish_count_ == 1) {
      dish_locs_.erase(dish);
      --num_tables_;
      --num_customers_;
      return -1;
    } else {
      int delta = 0;
      // sample customer to remove UNIFORMLY. that is, do NOT use the discount
      // here. if you do, it will introduce (unwanted) bias!
      double r = rng->next() * loc.total_dish_count_;
      --loc.total_dish_count_;
      for (typename std::list<unsigned>::iterator ti = loc.table_counts_.begin();
           ti != loc.table_counts_.end(); ++ti) {
        r -= *ti;
        if (r <= 0.0) {
          if ((--(*ti)) == 0) {
            --num_tables_;
            delta = -1;
            loc.table_counts_.erase(ti);
          }
          break;
        }
      }
      if (r > 0.0) {
        std::cerr << "Serious error: r=" << r << std::endl;
        Print(&std::cerr);
        assert(r <= 0.0);
      }
      --num_customers_;
      return delta;
    }
  }

  double prob(const Dish& dish, const double& p0) const {
    const typename std::tr1::unordered_map<Dish, DishLocations, DishHash>::const_iterator it = dish_locs_.find(dish);
    const double r = num_tables_ * discount_ + concentration_;
    if (it == dish_locs_.end()) {
      return r * p0 / (num_customers_ + concentration_);
    } else {
      return (it->second.total_dish_count_ - discount_ * it->second.table_counts_.size() + r * p0) /
               (num_customers_ + concentration_);
    }
  }

  template <typename T>
  T probT(const Dish& dish, const T& p0) const {
    const typename std::tr1::unordered_map<Dish, DishLocations, DishHash>::const_iterator it = dish_locs_.find(dish);
    const T r = T(num_tables_ * discount_ + concentration_);
    if (it == dish_locs_.end()) {
      return r * p0 / T(num_customers_ + concentration_);
    } else {
      return (T(it->second.total_dish_count_ - discount_ * it->second.table_counts_.size()) + r * p0) /
               T(num_customers_ + concentration_);
    }
  }

  double log_crp_prob() const {
    return log_crp_prob(discount_, concentration_);
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
  double log_crp_prob(const double& discount, const double& concentration) const {
    double lp = 0.0;
    if (has_discount_prior())
      lp = log_beta_density(discount, discount_prior_alpha_, discount_prior_beta_);
    if (has_concentration_prior())
      lp += log_gamma_density(concentration, concentration_prior_shape_, concentration_prior_rate_);
    assert(lp <= 0.0);
    if (num_customers_) {
      if (discount > 0.0) {
        const double r = lgamma(1.0 - discount);
        lp += lgamma(concentration) - lgamma(concentration + num_customers_)
             + num_tables_ * log(discount) + lgamma(concentration / discount + num_tables_)
             - lgamma(concentration / discount);
        assert(std::isfinite(lp));
        for (typename std::tr1::unordered_map<Dish, DishLocations, DishHash>::const_iterator it = dish_locs_.begin();
             it != dish_locs_.end(); ++it) {
          const DishLocations& cur = it->second;
          for (std::list<unsigned>::const_iterator ti = cur.table_counts_.begin(); ti != cur.table_counts_.end(); ++ti) {
            lp += lgamma(*ti - discount) - r;
          }
        }
      } else {
        assert(!"not implemented yet");
      }
    }
    assert(std::isfinite(lp));
    return lp;
  }

  void resample_hyperparameters(MT19937* rng, const unsigned nloop = 5, const unsigned niterations = 10) {
    assert(has_discount_prior() || has_concentration_prior());
    DiscountResampler dr(*this);
    ConcentrationResampler cr(*this);
    for (int iter = 0; iter < nloop; ++iter) {
      if (has_concentration_prior()) {
        concentration_ = slice_sampler1d(cr, concentration_, *rng, 0.0,
                               std::numeric_limits<double>::infinity(), 0.0, niterations, 100*niterations);
      }
      if (has_discount_prior()) {
        discount_ = slice_sampler1d(dr, discount_, *rng, std::numeric_limits<double>::min(),
                               1.0, 0.0, niterations, 100*niterations);
      }
    }
    concentration_ = slice_sampler1d(cr, concentration_, *rng, 0.0,
                             std::numeric_limits<double>::infinity(), 0.0, niterations, 100*niterations);
  }

  struct DiscountResampler {
    DiscountResampler(const CCRP& crp) : crp_(crp) {}
    const CCRP& crp_;
    double operator()(const double& proposed_discount) const {
      return crp_.log_crp_prob(proposed_discount, crp_.concentration_);
    }
  };

  struct ConcentrationResampler {
    ConcentrationResampler(const CCRP& crp) : crp_(crp) {}
    const CCRP& crp_;
    double operator()(const double& proposed_concentration) const {
      return crp_.log_crp_prob(crp_.discount_, proposed_concentration);
    }
  };

  struct DishLocations {
    DishLocations() : total_dish_count_() {}
    unsigned total_dish_count_;        // customers at all tables with this dish
    std::list<unsigned> table_counts_; // list<> gives O(1) deletion and insertion, which we want
                                       // .size() is the number of tables for this dish
  };

  void Print(std::ostream* out) const {
    std::cerr << "PYP(d=" << discount_ << ",c=" << concentration_ << ") customers=" << num_customers_ << std::endl;
    for (typename std::tr1::unordered_map<Dish, DishLocations, DishHash>::const_iterator it = dish_locs_.begin();
         it != dish_locs_.end(); ++it) {
      (*out) << it->first << " (" << it->second.total_dish_count_ << " on " << it->second.table_counts_.size() << " tables): ";
      for (typename std::list<unsigned>::const_iterator i = it->second.table_counts_.begin();
           i != it->second.table_counts_.end(); ++i) {
        (*out) << " " << *i;
      }
      (*out) << std::endl;
    }
  }

  typedef typename std::tr1::unordered_map<Dish, DishLocations, DishHash>::const_iterator const_iterator;
  const_iterator begin() const {
    return dish_locs_.begin();
  }
  const_iterator end() const {
    return dish_locs_.end();
  }

  unsigned num_tables_;
  unsigned num_customers_;
  std::tr1::unordered_map<Dish, DishLocations, DishHash> dish_locs_;

  double discount_;
  double concentration_;

  // optional beta prior on discount_ (NaN if no prior)
  double discount_prior_alpha_;
  double discount_prior_beta_;

  // optional gamma prior on concentration_ (NaN if no prior)
  double concentration_prior_shape_;
  double concentration_prior_rate_;
};

template <typename T,typename H>
std::ostream& operator<<(std::ostream& o, const CCRP<T,H>& c) {
  c.Print(&o);
  return o;
}

#endif
