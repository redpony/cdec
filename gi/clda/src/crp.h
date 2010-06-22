#ifndef _CRP_H_
#define _CRP_H_

// shamelessly adapted from code by Phil Blunsom and Trevor Cohn
// There are TWO CRP classes here: CRPWithTableTracking tracks the
// (expected) number of customers per table, and CRP just tracks
// the number of customers / dish.
// If you are implementing a HDP model, you should use CRP for the
// base distribution and CRPWithTableTracking for the dependent
// distribution.

#include <iostream>
#include <map>
#include <boost/functional/hash.hpp>
#include <tr1/unordered_map>

#include "prob.h"
#include "sampler.h"   // RNG

template <typename DishType, typename Hash = boost::hash<DishType> >
class CRP {
 public:
  CRP(double alpha) : alpha_(alpha), palpha_(alpha), total_customers_() {}
  void increment(const DishType& dish);
  void decrement(const DishType& dish);
  void erase(const DishType& dish) {
    counts_.erase(dish);
  }
  inline int count(const DishType& dish) const {
    const typename MapType::const_iterator i = counts_.find(dish);
    if (i == counts_.end()) return 0; else return i->second;
  }
  inline prob_t prob(const DishType& dish) const {
    return (prob_t(count(dish) + alpha_)) / prob_t(total_customers_ + alpha_);
  }
  inline prob_t prob(const DishType& dish, const prob_t& p0) const {
    return (prob_t(count(dish)) + palpha_ * p0) / prob_t(total_customers_ + alpha_);
  }
 private:
  typedef std::tr1::unordered_map<DishType, int, Hash> MapType;
  MapType counts_;
  const double alpha_;
  const prob_t palpha_;
  int total_customers_;
};

template <typename Dish, typename Hash>
void CRP<Dish,Hash>::increment(const Dish& dish) {
  ++counts_[dish];
  ++total_customers_;
}

template <typename Dish, typename Hash>
void CRP<Dish,Hash>::decrement(const Dish& dish) {
  typename MapType::iterator i = counts_.find(dish);
  assert(i != counts_.end());
  if (--i->second == 0)
    counts_.erase(i);
  --total_customers_;
}

template <typename DishType, typename Hash = boost::hash<DishType>, typename RNG = MT19937>
class CRPWithTableTracking {
 public:
  CRPWithTableTracking(double alpha, RNG* rng) :
    alpha_(alpha), palpha_(alpha), total_customers_(),
    total_tables_(), rng_(rng) {}

  // seat a customer for dish d, returns the delta in tables
  // with customers
  int increment(const DishType& d, const prob_t& p0 = prob_t::One());
  int decrement(const DishType& d);
  void erase(const DishType& dish);

  inline int count(const DishType& dish) const {
    const typename MapType::const_iterator i = counts_.find(dish);
    if (i == counts_.end()) return 0; else return i->second.count_;
  }
  inline prob_t prob(const DishType& dish) const {
    return (prob_t(count(dish) + alpha_)) / prob_t(total_customers_ + alpha_);
  }
  inline prob_t prob(const DishType& dish, const prob_t& p0) const {
    return (prob_t(count(dish)) + palpha_ * p0) / prob_t(total_customers_ + alpha_);
  }
 private:
  struct TableInfo {
    TableInfo() : count_(), tables_() {}
    int count_;          // total customers eating dish
    int tables_;         // total tables labeled with dish
    std::map<int, int> table_histogram_; // num customers at table -> number tables
  };
  typedef std::tr1::unordered_map<DishType, TableInfo, Hash> MapType;

  inline prob_t prob_share_table(const double& customer_count) const {
    if (customer_count)
      return prob_t(customer_count) / prob_t(customer_count + alpha_);
    else
      return prob_t::Zero();
  }
  inline prob_t prob_new_table(const double& customer_count, const prob_t& p0) const {
    if (customer_count)
      return palpha_ * p0 / prob_t(customer_count + alpha_);
    else
      return p0;
  }

  MapType counts_;
  const double alpha_;
  const prob_t palpha_;
  int total_customers_;
  int total_tables_;
  RNG* rng_;
};

template <typename Dish, typename Hash, typename RNG>
int CRPWithTableTracking<Dish,Hash,RNG>::increment(const Dish& dish, const prob_t& p0) {
  TableInfo& tc = counts_[dish];

  //std::cerr << "\nincrement for " << dish << " with p0 " << p0 << "\n";
  //std::cerr << "\tBEFORE histogram: " << tc.table_histogram_ << " ";
  //std::cerr << "count: " << tc.count_ << " ";
  //std::cerr << "tables: " << tc.tables_ << "\n";

  // seated at a new or existing table?
  prob_t pshare = prob_share_table(tc.count_);
  prob_t pnew = prob_new_table(tc.count_, p0);

  //std::cerr << "\t\tP0 " << p0 << " count(dish) " << count(dish)
  //  << " tables " << tc
  //  << " p(share) " << pshare << " p(new) " << pnew << "\n";

  int delta = 0;
  if (tc.count_ == 0 || rng_->SelectSample(pshare, pnew) == 1) {
    // assign to a new table
    ++tc.tables_;
    ++tc.table_histogram_[1];
    ++total_tables_;
    delta = 1;
  } else {
    // can't share a table if there are no other customers
    assert(tc.count_ > 0);

    // randomly assign to an existing table
    // remove constant denominator from inner loop
    int r = static_cast<int>(rng_->next() * tc.count_);
    for (std::map<int,int>::iterator hit = tc.table_histogram_.begin();
         hit != tc.table_histogram_.end(); ++hit) {
      r -= hit->first * hit->second;
      if (r <= 0) {
        ++tc.table_histogram_[hit->first+1];
        --hit->second;
        if (hit->second == 0)
          tc.table_histogram_.erase(hit);
        break;
      }
    }
    if (r > 0) { 
      std::cerr << "CONSISTENCY ERROR: " << tc.count_ << std::endl;
      std::cerr << pshare << std::endl;
      std::cerr << pnew << std::endl;
      std::cerr << r << std::endl;
      abort();
    }
  }
  ++tc.count_;
  ++total_customers_;
  return delta;
}

template <typename Dish, typename Hash, typename RNG>
int CRPWithTableTracking<Dish,Hash,RNG>::decrement(const Dish& dish) {
  typename MapType::iterator i = counts_.find(dish);
  if(i == counts_.end()) {
    std::cerr << "MISSING DISH: " << dish << std::endl;
    abort();
  }

  int delta = 0;
  TableInfo &tc = i->second;

  //std::cout << "\ndecrement for " << dish << " with p0 " << p0 << "\n";
  //std::cout << "\tBEFORE histogram: " << tc.table_histogram << " ";
  //std::cout << "count: " << count(dish) << " ";
  //std::cout << "tables: " << tc.tables << "\n";

  int r = static_cast<int>(rng_->next() * tc.count_);
  //std::cerr << "FOO: " << r << std::endl;
  for (std::map<int,int>::iterator hit = tc.table_histogram_.begin();
       hit != tc.table_histogram_.end(); ++hit) {
    r -= (hit->first * hit->second);
    if (r <= 0) {
      if (hit->first > 1)
        tc.table_histogram_[hit->first-1] += 1;
      else {
        --delta;
        --tc.tables_;
        --total_tables_;
      }

      --hit->second;
      if (hit->second == 0) tc.table_histogram_.erase(hit);
      break;
    }
  }

  assert(r <= 0);

  // remove the customer
  --tc.count_;
  --total_customers_;
  assert(tc.count_ >= 0);
  if (tc.count_ == 0) counts_.erase(i);
  return delta;
}

#endif
