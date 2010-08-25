#ifndef _CCRP_H_
#define _CCRP_H_

#include <cassert>
#include <cmath>
#include <list>
#include <iostream>
#include <vector>
#include <tr1/unordered_map>
#include <boost/functional/hash.hpp>
#include "sampler.h"

// Chinese restaurant process (Pitman-Yor parameters) with explicit table
// tracking.

template <typename Dish, typename DishHash = boost::hash<Dish> >
class CCRP {
 public:
  CCRP(double disc, double conc) : num_tables_(), num_customers_(), discount_(disc), concentration_(conc) {}

  int increment(const Dish& dish, const double& p0, MT19937* rng) {
    DishLocations& loc = dish_locs_[dish];
    bool share_table = false;
    if (loc.dish_count_) {
      const double p_empty = (concentration_ + num_tables_ * discount_) * p0;
      const double p_share = (loc.dish_count_ - loc.table_counts_.size() * discount_);
      share_table = rng->SelectSample(p_empty, p_share);
    }
    if (share_table) {
      double r = rng->next() * (loc.dish_count_ - loc.table_counts_.size() * discount_);
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
    ++loc.dish_count_;
    ++num_customers_;
    return (share_table ? 0 : 1);
  }

  int decrement(const Dish& dish, MT19937* rng) {
    DishLocations& loc = dish_locs_[dish];
    assert(loc.dish_count_);
    if (loc.dish_count_ == 1) {
      dish_locs_.erase(dish);
      --num_tables_;
      --num_customers_;
      return -1;
    } else {
      int delta = 0;
      double r = rng->next() * (loc.dish_count_ - loc.table_counts_.size() * discount_);
      --loc.dish_count_;
      for (typename std::list<unsigned>::iterator ti = loc.table_counts_.begin();
           ti != loc.table_counts_.end(); ++ti) {
        r -= (*ti - discount_);
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
      return (it->second.dish_count_ - discount_ * it->second.table_counts_.size() + r * p0) /
               (num_customers_ + concentration_);
    }
  }

  double llh() const {
    if (num_customers_) {
      std::cerr << "not implemented\n";
      return 0.0;
    } else {
      return 0.0;
    }
  }

  struct DishLocations {
    DishLocations() : dish_count_() {}
    unsigned dish_count_;
    std::list<unsigned> table_counts_; // list<> gives O(1) deletion and insertion, which we want
  };

  void Print(std::ostream* out) const {
    for (typename std::tr1::unordered_map<Dish, DishLocations, DishHash>::const_iterator it = dish_locs_.begin();
         it != dish_locs_.end(); ++it) {
      (*out) << it->first << " (" << it->second.dish_count_ << " on " << it->second.table_counts_.size() << " tables): ";
      for (typename std::list<unsigned>::const_iterator i = it->second.table_counts_.begin();
           i != it->second.table_counts_.end(); ++i) {
        (*out) << " " << *i;
      }
      (*out) << std::endl;
    }
  }

  unsigned num_tables_;
  unsigned num_customers_;
  std::tr1::unordered_map<Dish, DishLocations, DishHash> dish_locs_;

  double discount_;
  double concentration_;
};

template <typename T,typename H>
std::ostream& operator<<(std::ostream& o, const CCRP<T,H>& c) {
  c.Print(&o);
  return o;
}

#endif
