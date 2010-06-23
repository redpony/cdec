#ifndef _CRP_H_
#define _CRP_H_

// shamelessly adapted from code by Phil Blunsom and Trevor Cohn

#include <map>
#include <boost/functional/hash.hpp>
#include <tr1/unordered_map>

#include "prob.h"

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

#endif
