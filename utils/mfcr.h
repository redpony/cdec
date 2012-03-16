#ifndef _MFCR_H_
#define _MFCR_H_

#include <algorithm>
#include <numeric>
#include <cassert>
#include <cmath>
#include <list>
#include <iostream>
#include <vector>
#include <iterator>
#include <tr1/unordered_map>
#include <boost/functional/hash.hpp>
#include "sampler.h"
#include "slice_sampler.h"
#include "m.h"

struct TableCount {
  TableCount() : count(), floor() {}
  TableCount(int c, int f) : count(c), floor(f) {
    assert(f >= 0);
  }
  int count;               // count or delta (may be 0, <0, or >0)
  unsigned char floor;     // from which floor?
};
 
std::ostream& operator<<(std::ostream& o, const TableCount& tc) {
  return o << "[c=" << tc.count << " floor=" << static_cast<unsigned int>(tc.floor) << ']';
}

// Multi-Floor Chinese Restaurant as proposed by Wood & Teh (AISTATS, 2009) to simulate
// graphical Pitman-Yor processes.
// http://jmlr.csail.mit.edu/proceedings/papers/v5/wood09a/wood09a.pdf
//
// Implementation is based on Blunsom, Cohn, Goldwater, & Johnson (ACL 2009) and code
// referenced therein.
// http://www.aclweb.org/anthology/P/P09/P09-2085.pdf
//
template <unsigned Floors, typename Dish, typename DishHash = boost::hash<Dish> >
class MFCR {
 public:

  MFCR(double d, double strength) :
    num_tables_(),
    num_customers_(),
    discount_(d),
    strength_(strength),
    discount_prior_strength_(std::numeric_limits<double>::quiet_NaN()),
    discount_prior_beta_(std::numeric_limits<double>::quiet_NaN()),
    strength_prior_shape_(std::numeric_limits<double>::quiet_NaN()),
    strength_prior_rate_(std::numeric_limits<double>::quiet_NaN()) { check_hyperparameters(); }

  MFCR(double discount_strength, double discount_beta, double strength_shape, double strength_rate, double d = 0.9, double strength = 10.0) :
    num_tables_(),
    num_customers_(),
    discount_(d),
    strength_(strength),
    discount_prior_strength_(discount_strength),
    discount_prior_beta_(discount_beta),
    strength_prior_shape_(strength_shape),
    strength_prior_rate_(strength_rate) { check_hyperparameters(); }

  void check_hyperparameters() {
    if (discount_ < 0.0 || discount_ >= 1.0) {
      std::cerr << "Bad discount: " << discount_ << std::endl;
      abort();
    }
    if (strength_ <= -discount_) {
      std::cerr << "Bad strength: " << strength_ << " (discount=" << discount_ << ")" << std::endl;
      abort();
    }
  }

  double discount() const { return discount_; }
  double strength() const { return strength_; }
  void set_hyperparameters(double d, double s) {
    discount_ = d; strength_ = s;
    check_hyperparameters();
  }
  void set_discount(double d) { discount_ = d; check_hyperparameters(); }
  void set_strength(double a) { strength_ = a; check_hyperparameters(); }

  bool has_discount_prior() const {
    return !std::isnan(discount_prior_strength_);
  }

  bool has_strength_prior() const {
    return !std::isnan(strength_prior_shape_);
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

  // this is not terribly efficient but it should not typically be necessary to execute this query
  unsigned num_tables(const Dish& dish, const unsigned floor) const {
    const typename std::tr1::unordered_map<Dish, DishLocations, DishHash>::const_iterator it = dish_locs_.find(dish);
    if (it == dish_locs_.end()) return 0;
    unsigned c = 0;
    for (typename std::list<TableCount>::const_iterator i = it->second.table_counts_.begin();
         i != it->second.table_counts_.end(); ++i) {
      if (i->floor == floor) ++c;
    }
    return c;
  }

  unsigned num_customers() const {
    return num_customers_;
  }

  unsigned num_customers(const Dish& dish) const {
    const typename std::tr1::unordered_map<Dish, DishLocations, DishHash>::const_iterator it = dish_locs_.find(dish);
    if (it == dish_locs_.end()) return 0;
    return it->total_dish_count_;
  }

  // returns (delta, floor) indicating whether a new table (delta) was opened and on which floor
  template <class InputIterator, class InputIterator2>
  TableCount increment(const Dish& dish, InputIterator p0s, InputIterator2 lambdas, MT19937* rng) {
    DishLocations& loc = dish_locs_[dish];
    // marg_p0 = marginal probability of opening a new table on any floor with label dish
    typedef typename std::iterator_traits<InputIterator>::value_type F;
    const F marg_p0 = std::inner_product(p0s, p0s + Floors, lambdas, F(0.0));
    assert(marg_p0 <= F(1.0001));
    int floor = -1;
    bool share_table = false;
    if (loc.total_dish_count_) {
      const F p_empty = F(strength_ + num_tables_ * discount_) * marg_p0;
      const F p_share = F(loc.total_dish_count_ - loc.table_counts_.size() * discount_);
      share_table = rng->SelectSample(p_empty, p_share);
    }
    if (share_table) {
      // this can be done with doubles since P0 (which may be tiny) is not involved
      double r = rng->next() * (loc.total_dish_count_ - loc.table_counts_.size() * discount_);
      for (typename std::list<TableCount>::iterator ti = loc.table_counts_.begin();
           ti != loc.table_counts_.end(); ++ti) {
        r -= ti->count - discount_;
        if (r <= 0.0) {
          ++ti->count;
          floor = ti->floor;
          break;
        }
      }
      if (r > 0.0) {
        std::cerr << "Serious error: r=" << r << std::endl;
        Print(&std::cerr);
        assert(r <= 0.0);
      }
    } else { // sit at currently empty table -- must sample what floor
      if (Floors == 1) {
        floor = 0;
      } else {
        F r = F(rng->next()) * marg_p0;
        for (unsigned i = 0; i < Floors; ++i) {
          r -= (*p0s) * (*lambdas);
          ++p0s;
          ++lambdas;
          if (r <= F(0.0)) {
            floor = i;
            break;
          }
        }
      }
      assert(floor >= 0);
      loc.table_counts_.push_back(TableCount(1, floor));
      ++num_tables_;
    }
    ++loc.total_dish_count_;
    ++num_customers_;
    return (share_table ? TableCount(0, floor) : TableCount(1, floor));
  }

  // returns first = -1 or 0, indicating whether a table was closed, and on what floor (second)
  TableCount decrement(const Dish& dish, MT19937* rng) {
    DishLocations& loc = dish_locs_[dish];
    assert(loc.total_dish_count_);
    int floor = -1;
    int delta = 0;
    if (loc.total_dish_count_ == 1) {
      floor = loc.table_counts_.front().floor;
      dish_locs_.erase(dish);
      --num_tables_;
      --num_customers_;
      delta = -1;
    } else {
      // sample customer to remove UNIFORMLY. that is, do NOT use the d
      // here. if you do, it will introduce (unwanted) bias!
      double r = rng->next() * loc.total_dish_count_;
      --loc.total_dish_count_;
      --num_customers_;
      for (typename std::list<TableCount>::iterator ti = loc.table_counts_.begin();
           ti != loc.table_counts_.end(); ++ti) {
        r -= ti->count;
        if (r <= 0.0) {
          floor = ti->floor;
          if ((--ti->count) == 0) {
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
    }
    return TableCount(delta, floor);
  }

  template <class InputIterator, class InputIterator2>
  typename std::iterator_traits<InputIterator>::value_type prob(const Dish& dish, InputIterator p0s, InputIterator2 lambdas) const {
    typedef typename std::iterator_traits<InputIterator>::value_type F;
    const F marg_p0 = std::inner_product(p0s, p0s + Floors, lambdas, F(0.0));
    assert(marg_p0 <= F(1.0001));
    const typename std::tr1::unordered_map<Dish, DishLocations, DishHash>::const_iterator it = dish_locs_.find(dish);
    const F r = F(num_tables_ * discount_ + strength_);
    if (it == dish_locs_.end()) {
      return r * marg_p0 / F(num_customers_ + strength_);
    } else {
      return (F(it->second.total_dish_count_ - discount_ * it->second.table_counts_.size()) + F(r * marg_p0)) /
               F(num_customers_ + strength_);
    }
  }

  double log_crp_prob() const {
    return log_crp_prob(discount_, strength_);
  }

  // taken from http://en.wikipedia.org/wiki/Chinese_restaurant_process
  // does not include draws from G_w's
  double log_crp_prob(const double& discount, const double& strength) const {
    double lp = 0.0;
    if (has_discount_prior())
      lp = Md::log_beta_density(discount, discount_prior_strength_, discount_prior_beta_);
    if (has_strength_prior())
      lp += Md::log_gamma_density(strength + discount, strength_prior_shape_, strength_prior_rate_);
    assert(lp <= 0.0);
    if (num_customers_) {
      if (discount > 0.0) {
        const double r = lgamma(1.0 - discount);
        if (strength)
          lp += lgamma(strength) - lgamma(strength / discount);
        lp += - lgamma(strength + num_customers_)
             + num_tables_ * log(discount) + lgamma(strength / discount + num_tables_);
        assert(std::isfinite(lp));
        for (typename std::tr1::unordered_map<Dish, DishLocations, DishHash>::const_iterator it = dish_locs_.begin();
             it != dish_locs_.end(); ++it) {
          const DishLocations& cur = it->second;
          for (std::list<TableCount>::const_iterator ti = cur.table_counts_.begin(); ti != cur.table_counts_.end(); ++ti) {
            lp += lgamma(ti->count - discount) - r;
          }
        }
      } else if (!discount) { // discount == 0.0
        lp += lgamma(strength) + num_tables_ * log(strength) - lgamma(strength + num_tables_);
        assert(std::isfinite(lp));
        for (typename std::tr1::unordered_map<Dish, DishLocations, DishHash>::const_iterator it = dish_locs_.begin();
             it != dish_locs_.end(); ++it) {
          const DishLocations& cur = it->second;
          lp += lgamma(cur.table_counts_.size());
        }
      } else {
        assert(!"discount less than 0 detected!");
      }
    }
    assert(std::isfinite(lp));
    return lp;
  }

  void resample_hyperparameters(MT19937* rng, const unsigned nloop = 5, const unsigned niterations = 10) {
    assert(has_discount_prior() || has_strength_prior());
    DiscountResampler dr(*this);
    StrengthResampler sr(*this);
    for (int iter = 0; iter < nloop; ++iter) {
      if (has_strength_prior()) {
        strength_ = slice_sampler1d(sr, strength_, *rng, -discount_,
                               std::numeric_limits<double>::infinity(), 0.0, niterations, 100*niterations);
      }
      if (has_discount_prior()) {
        double min_discount = std::numeric_limits<double>::min();
        if (strength_ < 0.0) min_discount -= strength_;
        discount_ = slice_sampler1d(dr, discount_, *rng, min_discount,
                               1.0, 0.0, niterations, 100*niterations);
      }
    }
    strength_ = slice_sampler1d(sr, strength_, *rng, -discount_,
                             std::numeric_limits<double>::infinity(), 0.0, niterations, 100*niterations);
  }

  struct DiscountResampler {
    DiscountResampler(const MFCR& crp) : crp_(crp) {}
    const MFCR& crp_;
    double operator()(const double& proposed_d) const {
      return crp_.log_crp_prob(proposed_d, crp_.strength_);
    }
  };

  struct StrengthResampler {
    StrengthResampler(const MFCR& crp) : crp_(crp) {}
    const MFCR& crp_;
    double operator()(const double& proposediscount_strength) const {
      return crp_.log_crp_prob(crp_.discount_, proposediscount_strength);
    }
  };

  struct DishLocations {
    DishLocations() : total_dish_count_() {}
    unsigned total_dish_count_;          // customers at all tables with this dish
    std::list<TableCount> table_counts_; // list<> gives O(1) deletion and insertion, which we want
                                         // .size() is the number of tables for this dish
  };

  void Print(std::ostream* out) const {
    (*out) << "MFCR<" << Floors << ">(d=" << discount_ << ",strength=" << strength_ << ") customers=" << num_customers_ << std::endl;
    for (typename std::tr1::unordered_map<Dish, DishLocations, DishHash>::const_iterator it = dish_locs_.begin();
         it != dish_locs_.end(); ++it) {
      (*out) << it->first << " (" << it->second.total_dish_count_ << " on " << it->second.table_counts_.size() << " tables): ";
      for (typename std::list<TableCount>::const_iterator i = it->second.table_counts_.begin();
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
  double strength_;

  // optional beta prior on discount_ (NaN if no prior)
  double discount_prior_strength_;
  double discount_prior_beta_;

  // optional gamma prior on strength_ (NaN if no prior)
  double strength_prior_shape_;
  double strength_prior_rate_;
};

template <unsigned N,typename T,typename H>
std::ostream& operator<<(std::ostream& o, const MFCR<N,T,H>& c) {
  c.Print(&o);
  return o;
}

#endif
