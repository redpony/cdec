#ifndef _MFCR_H_
#define _MFCR_H_

#include <algorithm>
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
template <typename Dish, typename DishHash = boost::hash<Dish> >
class MFCR {
 public:

  MFCR(unsigned num_floors, double d, double alpha) :
    num_floors_(num_floors),
    num_tables_(),
    num_customers_(),
    d_(d),
    alpha_(alpha),
    d_prior_alpha_(std::numeric_limits<double>::quiet_NaN()),
    d_prior_beta_(std::numeric_limits<double>::quiet_NaN()),
    alpha_prior_shape_(std::numeric_limits<double>::quiet_NaN()),
    alpha_prior_rate_(std::numeric_limits<double>::quiet_NaN()) {}

  MFCR(unsigned num_floors, double d_alpha, double d_beta, double alpha_shape, double alpha_rate, double d = 0.9, double alpha = 10.0) :
    num_floors_(num_floors),
    num_tables_(),
    num_customers_(),
    d_(d),
    alpha_(alpha),
    d_prior_alpha_(d_alpha),
    d_prior_beta_(d_beta),
    alpha_prior_shape_(alpha_shape),
    alpha_prior_rate_(alpha_rate) {}

  double d() const { return d_; }
  double alpha() const { return alpha_; }

  bool has_d_prior() const {
    return !std::isnan(d_prior_alpha_);
  }

  bool has_alpha_prior() const {
    return !std::isnan(alpha_prior_shape_);
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
  TableCount increment(const Dish& dish, const std::vector<double>& p0s, const std::vector<double>& lambdas, MT19937* rng) {
    assert(p0s.size() == num_floors_);
    assert(lambdas.size() == num_floors_);

    DishLocations& loc = dish_locs_[dish];
    // marg_p0 = marginal probability of opening a new table on any floor with label dish
    const double marg_p0 = std::inner_product(p0s.begin(), p0s.end(), lambdas.begin(), 0.0);
    assert(marg_p0 <= 1.0);
    int floor = -1;
    bool share_table = false;
    if (loc.total_dish_count_) {
      const double p_empty = (alpha_ + num_tables_ * d_) * marg_p0;
      const double p_share = (loc.total_dish_count_ - loc.table_counts_.size() * d_);
      share_table = rng->SelectSample(p_empty, p_share);
    }
    if (share_table) {
      double r = rng->next() * (loc.total_dish_count_ - loc.table_counts_.size() * d_);
      for (typename std::list<TableCount>::iterator ti = loc.table_counts_.begin();
           ti != loc.table_counts_.end(); ++ti) {
        r -= ti->count - d_;
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
      double r = rng->next() * marg_p0;
      for (unsigned i = 0; i < p0s.size(); ++i) {
        r -= p0s[i] * lambdas[i];
        if (r <= 0.0) {
          floor = i;
          break;
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

  double prob(const Dish& dish, const std::vector<double>& p0s, const std::vector<double>& lambdas) const {
    assert(p0s.size() == num_floors_);
    assert(lambdas.size() == num_floors_);
    const double marg_p0 = std::inner_product(p0s.begin(), p0s.end(), lambdas.begin(), 0.0);
    assert(marg_p0 <= 1.0);
    const typename std::tr1::unordered_map<Dish, DishLocations, DishHash>::const_iterator it = dish_locs_.find(dish);
    const double r = num_tables_ * d_ + alpha_;
    if (it == dish_locs_.end()) {
      return r * marg_p0 / (num_customers_ + alpha_);
    } else {
      return (it->second.total_dish_count_ - d_ * it->second.table_counts_.size() + r * marg_p0) /
               (num_customers_ + alpha_);
    }
  }

  double log_crp_prob() const {
    return log_crp_prob(d_, alpha_);
  }

  // taken from http://en.wikipedia.org/wiki/Chinese_restaurant_process
  // does not include draws from G_w's
  double log_crp_prob(const double& d, const double& alpha) const {
    double lp = 0.0;
    if (has_d_prior())
      lp = Md::log_beta_density(d, d_prior_alpha_, d_prior_beta_);
    if (has_alpha_prior())
      lp += Md::log_gamma_density(alpha, alpha_prior_shape_, alpha_prior_rate_);
    assert(lp <= 0.0);
    if (num_customers_) {
      if (d > 0.0) {
        const double r = lgamma(1.0 - d);
        lp += lgamma(alpha) - lgamma(alpha + num_customers_)
             + num_tables_ * log(d) + lgamma(alpha / d + num_tables_)
             - lgamma(alpha / d);
        assert(std::isfinite(lp));
        for (typename std::tr1::unordered_map<Dish, DishLocations, DishHash>::const_iterator it = dish_locs_.begin();
             it != dish_locs_.end(); ++it) {
          const DishLocations& cur = it->second;
          for (std::list<TableCount>::const_iterator ti = cur.table_counts_.begin(); ti != cur.table_counts_.end(); ++ti) {
            lp += lgamma(ti->count - d) - r;
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
    assert(has_d_prior() || has_alpha_prior());
    DiscountResampler dr(*this);
    ConcentrationResampler cr(*this);
    for (int iter = 0; iter < nloop; ++iter) {
      if (has_alpha_prior()) {
        alpha_ = slice_sampler1d(cr, alpha_, *rng, 0.0,
                               std::numeric_limits<double>::infinity(), 0.0, niterations, 100*niterations);
      }
      if (has_d_prior()) {
        d_ = slice_sampler1d(dr, d_, *rng, std::numeric_limits<double>::min(),
                               1.0, 0.0, niterations, 100*niterations);
      }
    }
    alpha_ = slice_sampler1d(cr, alpha_, *rng, 0.0,
                             std::numeric_limits<double>::infinity(), 0.0, niterations, 100*niterations);
  }

  struct DiscountResampler {
    DiscountResampler(const MFCR& crp) : crp_(crp) {}
    const MFCR& crp_;
    double operator()(const double& proposed_d) const {
      return crp_.log_crp_prob(proposed_d, crp_.alpha_);
    }
  };

  struct ConcentrationResampler {
    ConcentrationResampler(const MFCR& crp) : crp_(crp) {}
    const MFCR& crp_;
    double operator()(const double& proposed_alpha) const {
      return crp_.log_crp_prob(crp_.d_, proposed_alpha);
    }
  };

  struct DishLocations {
    DishLocations() : total_dish_count_() {}
    unsigned total_dish_count_;          // customers at all tables with this dish
    std::list<TableCount> table_counts_; // list<> gives O(1) deletion and insertion, which we want
                                         // .size() is the number of tables for this dish
  };

  void Print(std::ostream* out) const {
    (*out) << "MFCR(d=" << d_ << ",alpha=" << alpha_ << ") customers=" << num_customers_ << std::endl;
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

  unsigned num_floors_;
  unsigned num_tables_;
  unsigned num_customers_;
  std::tr1::unordered_map<Dish, DishLocations, DishHash> dish_locs_;

  double d_;
  double alpha_;

  // optional beta prior on d_ (NaN if no prior)
  double d_prior_alpha_;
  double d_prior_beta_;

  // optional gamma prior on alpha_ (NaN if no prior)
  double alpha_prior_shape_;
  double alpha_prior_rate_;
};

template <typename T,typename H>
std::ostream& operator<<(std::ostream& o, const MFCR<T,H>& c) {
  c.Print(&o);
  return o;
}

#endif
