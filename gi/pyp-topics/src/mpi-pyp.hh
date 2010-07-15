#ifndef _pyp_hh
#define _pyp_hh

#include <math.h>
#include <map>
#include <tr1/unordered_map>
//#include <google/sparse_hash_map>

#include <boost/random/uniform_real.hpp>
#include <boost/random/variate_generator.hpp>
#include <boost/random/mersenne_twister.hpp>

#include "pyp.h"
#include "log_add.h"
#include "slice-sampler.h"
#include "mt19937ar.h"

//
// Pitman-Yor process with customer and table tracking
//

template <typename Dish, typename Hash=std::tr1::hash<Dish> >
class MPIPYP : public PYP<Dish, Hash> {
public:
  MPIPYP(double a, double b, Hash hash=Hash());

  virtual int increment(Dish d, double p0);
  virtual int decrement(Dish d);

  void clear();

  void reset_deltas() { m_count_delta.clear(); }

private:
  typedef std::map<Dish, int> dish_delta_type;
  typedef std::map<Dish, TableCounter> table_delta_type;

  dish_delta_type m_count_delta;
  table_delta_type m_table_delta;
};

template <typename Dish, typename Hash>
MPIPYP<Dish,Hash>::MPIPYP(double a, double b, Hash)
: PYP(a, b, Hash) {}

template <typename Dish, typename Hash>
int 
MPIPYP<Dish,Hash>::increment(Dish dish, double p0) {
  int delta = PYP<Dish,Hash>::increment(dish, p0);

  return delta;
}

template <typename Dish, typename Hash>
int 
MPIPYP<Dish,Hash>::decrement(Dish dish)
{
  int delta = PYP<Dish,Hash>::decrement(dish);
  return delta;
}

template <typename Dish, typename Hash>
void 
MPIPYP<Dish,Hash>::clear()
{
  PYP<Dish,Hash>::clear();
}

#endif
