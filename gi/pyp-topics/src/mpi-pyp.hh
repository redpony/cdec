#ifndef _mpipyp_hh
#define _mpipyp_hh

#include <math.h>
#include <map>
#include <tr1/unordered_map>
//#include <google/sparse_hash_map>

#include <boost/random/uniform_real.hpp>
#include <boost/random/variate_generator.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/serialization/map.hpp>
#include <boost/mpi.hpp>
#include <boost/mpi/environment.hpp>
#include <boost/mpi/communicator.hpp>
#include <boost/mpi/operations.hpp>


#include "pyp.hh"

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
  void reset_deltas();

  void synchronise();

private:
  typedef std::map<Dish, int> dish_delta_type;
  typedef std::map<Dish, typename PYP<Dish,Hash>::TableCounter> table_delta_type;

  dish_delta_type m_count_delta;
  table_delta_type m_table_delta;
};

template <typename Dish, typename Hash>
MPIPYP<Dish,Hash>::MPIPYP(double a, double b, Hash h)
: PYP<Dish,Hash>(a, b, 0, h) {}

template <typename Dish, typename Hash>
int 
MPIPYP<Dish,Hash>::increment(Dish dish, double p0) {
  int delta = 0;
  int table_joined=-1;
  typename PYP<Dish,Hash>::TableCounter &tc = PYP<Dish,Hash>::_dish_tables[dish];

  // seated on a new or existing table?
  int c = PYP<Dish,Hash>::count(dish); 
  int t = PYP<Dish,Hash>::num_tables(dish); 
  int T = PYP<Dish,Hash>::num_tables();
  double& a = PYP<Dish,Hash>::_a;
  double& b = PYP<Dish,Hash>::_b;
  double pshare = (c > 0) ? (c - a*t) : 0.0;
  double pnew = (b + a*T) * p0;
  assert (pshare >= 0.0);

  if (mt_genrand_res53() < pnew / (pshare + pnew)) {
    // assign to a new table
    tc.tables += 1;
    tc.table_histogram[1] += 1;
    PYP<Dish,Hash>::_total_tables += 1;
    delta = 1;
  }
  else {
    // randomly assign to an existing table
    // remove constant denominator from inner loop
    double r = mt_genrand_res53() * (c - a*t);
    for (std::map<int,int>::iterator
         hit = tc.table_histogram.begin();
         hit != tc.table_histogram.end(); ++hit) {
      r -= ((hit->first - a) * hit->second);
      if (r <= 0) {
        tc.table_histogram[hit->first+1] += 1;
        hit->second -= 1;
        if (hit->second == 0)
          tc.table_histogram.erase(hit);
        table_joined = hit->first+1;
        break;
      }
    }
    if (r > 0) {
      std::cerr << r << " " << c << " " << a << " " << t << std::endl;
      assert(false);
    }
    delta = 0;
  }

  std::tr1::unordered_map<Dish,int,Hash>::operator[](dish) += 1;
  //google::sparse_hash_map<Dish,int,Hash>::operator[](dish) += 1;
  PYP<Dish,Hash>::_total_customers += 1;

  // MPI Delta handling
  // track the customer entering
  typename dish_delta_type::iterator customer_it; 
  bool customer_insert_result; 
  boost::tie(customer_it, customer_insert_result) 
    = m_count_delta.insert(std::make_pair(dish,0)); 

  customer_it->second += 1;
  if (customer_it->second == 0)
    m_count_delta.erase(customer_it);

  // increment the histogram bar for the table joined
  if (!delta) {
    assert (table_joined >= 0);
    std::map<int,int> &histogram = m_table_delta[dish].table_histogram;
    typename std::map<int,int>::iterator table_it; bool table_insert_result; 
    boost::tie(table_it, table_insert_result) = histogram.insert(std::make_pair(table_joined,0)); 
    table_it->second += 1;
    if (table_it->second == 0) histogram.erase(table_it);

    // decrement the histogram bar for the table left 
    boost::tie(table_it, table_insert_result) = histogram.insert(std::make_pair(table_joined-1,0)); 
    table_it->second -= 1;
    if (table_it->second == 0) histogram.erase(table_it);
  }
  else {
    typename PYP<Dish,Hash>::TableCounter &delta_tc = m_table_delta[dish];
    delta_tc.tables += 1;
    delta_tc.table_histogram[1] += 1;
  }

  return delta;
}

template <typename Dish, typename Hash>
int 
MPIPYP<Dish,Hash>::decrement(Dish dish)
{
  typename std::tr1::unordered_map<Dish, int>::iterator dcit = find(dish);
  //typename google::sparse_hash_map<Dish, int>::iterator dcit = find(dish);
  if (dcit == PYP<Dish,Hash>::end()) {
    std::cerr << dish << std::endl;
    assert(false);
  } 

  int delta = 0, table_left=-1;

  typename std::tr1::unordered_map<Dish, typename PYP<Dish,Hash>::TableCounter>::iterator dtit 
    = PYP<Dish,Hash>::_dish_tables.find(dish);
  //typename google::sparse_hash_map<Dish, TableCounter>::iterator dtit = _dish_tables.find(dish);
  if (dtit == PYP<Dish,Hash>::_dish_tables.end()) {
    std::cerr << dish << std::endl;
    assert(false);
  } 
  typename PYP<Dish,Hash>::TableCounter &tc = dtit->second;

  double r = mt_genrand_res53() * PYP<Dish,Hash>::count(dish);
  for (std::map<int,int>::iterator hit = tc.table_histogram.begin();
       hit != tc.table_histogram.end(); ++hit) {
    r -= (hit->first * hit->second);
    if (r <= 0) {
      table_left = hit->first;
      if (hit->first > 1) {
        tc.table_histogram[hit->first-1] += 1;
      }
      else {
        delta = -1;
        tc.tables -= 1;
        PYP<Dish,Hash>::_total_tables -= 1;
      }

      hit->second -= 1;
      if (hit->second == 0) tc.table_histogram.erase(hit);
      break;
    }
  }
  if (r > 0) {
    std::cerr << r << " " << PYP<Dish,Hash>::count(dish) << " " << PYP<Dish,Hash>::_a << " " 
      << PYP<Dish,Hash>::num_tables(dish) << std::endl;
    assert(false);
  }

  // remove the customer
  dcit->second -= 1;
  PYP<Dish,Hash>::_total_customers -= 1;
  assert(dcit->second >= 0);
  if (dcit->second == 0) {
    PYP<Dish,Hash>::erase(dcit);
    PYP<Dish,Hash>::_dish_tables.erase(dtit);
  }

  typename dish_delta_type::iterator it; 
  bool insert_result; 
  boost::tie(it, insert_result) = m_count_delta.insert(std::make_pair(dish,0)); 

  it->second -= 1;

  if (it->second == 0)
    m_count_delta.erase(it);

  assert (table_left >= 0);
  typename PYP<Dish,Hash>::TableCounter& delta_tc = m_table_delta[dish];
  if (table_left > 1)
    delta_tc.table_histogram[table_left-1] += 1;
  else delta_tc.tables -= 1;

  std::map<int,int>::iterator tit = delta_tc.table_histogram.find(table_left);
  //assert (tit != delta_tc.table_histogram.end());
  tit->second -= 1;
  if (tit->second == 0) delta_tc.table_histogram.erase(tit);

  return delta;
}

template <typename Dish, typename Hash>
void 
MPIPYP<Dish,Hash>::clear() {
  PYP<Dish,Hash>::clear();
  reset_deltas();
}

template <typename Dish, typename Hash>
void 
MPIPYP<Dish,Hash>::reset_deltas() { 
  m_count_delta.clear(); 
  m_table_delta.clear();
}

template <typename Dish>
struct sum_maps {
  typedef std::map<Dish,int> map_type;
  map_type& operator() (map_type& l, map_type const & r) const {
    for (typename map_type::const_iterator it=r.begin(); it != r.end(); it++)
      l[it->first] += it->second;
    return l;
  }
};

// Needed Boost definitions
namespace boost { 
  namespace mpi {
    template <>
    struct is_commutative< sum_maps<int>, std::map<int,int> > : mpl::true_ {};
  }

  namespace serialization {
    template<class Archive>
    void serialize(Archive & ar, PYP<int>::TableCounter& t, const unsigned int version) {
      ar & t.table_histogram;
      ar & t.tables;
    }

  } // namespace serialization
} // namespace boost


template <typename Dish, typename Hash>
void 
MPIPYP<Dish,Hash>::synchronise() {
  boost::mpi::communicator world; 
  int rank = world.rank(), size = world.size();

  // communicate the customer count deltas
  dish_delta_type global_dish_delta; // the “merged” map
  boost::mpi::all_reduce(world, m_count_delta, global_dish_delta, sum_maps<Dish>());

  // update this restaurant
  for (typename dish_delta_type::const_iterator it=global_dish_delta.begin(); 
       it != global_dish_delta.end(); ++it) {
    std::tr1::unordered_map<Dish,int,Hash>::operator[](it->first) += (it->second - m_count_delta[it->first]);
    PYP<Dish,Hash>::_total_customers += (it->second - m_count_delta[it->first]);
    //std::cerr << "Process " << rank << " adding " <<  (it->second - m_count_delta[it->first]) << " customers." << std::endl;
  }

  // communicate the table count deltas
//  for (int process = 0; process < size; ++process) {
//    if (rank == process) {
//      // broadcast deltas
//      std::cerr << " -- Rank " << rank << " broadcasting -- " << std::endl;
//
//      boost::mpi::broadcast(world, m_table_delta, process);
//
//      std::cerr << " -- Rank " << rank << " done broadcasting -- " << std::endl;
//    }
//    else {
//      std::cerr << " -- Rank " << rank << " receiving -- " << std::endl;
//      // receive deltas
//      table_delta_type recv_table_delta;
//
//      boost::mpi::broadcast(world, recv_table_delta, process);
//
//      std::cerr << " -- Rank " << rank << " done receiving -- " << std::endl;
//
//      for (typename table_delta_type::const_iterator dish_it=recv_table_delta.begin(); 
//           dish_it != recv_table_delta.end(); ++dish_it) {
//        typename PYP<Dish,Hash>::TableCounter &tc = PYP<Dish,Hash>::_dish_tables[dish_it->first];
//
//        for (std::map<int,int>::const_iterator it=dish_it->second.table_histogram.begin(); 
//             it != dish_it->second.table_histogram.end(); ++it) {
//          tc.table_histogram[it->first] += it->second;
//        }
//        tc.tables += dish_it->second.tables;
//        PYP<Dish,Hash>::_total_tables += dish_it->second.tables;
//      }
//    }
//  }
//  std::cerr << " -- Done Reducing -- " << std::endl;

  reset_deltas();
}

#endif
