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
  typedef std::map<Dish, int> dish_delta_type;

  MPIPYP(double a, double b, Hash hash=Hash());

  template < typename Uniform01 >
    int increment(Dish d, double p0, Uniform01& rnd);
  template < typename Uniform01 >
    int decrement(Dish d, Uniform01& rnd);

  void clear();
  void reset_deltas();

  void synchronise(dish_delta_type* result);

private:
  typedef std::map<Dish, typename PYP<Dish,Hash>::TableCounter> table_delta_type;

  dish_delta_type m_count_delta;
  table_delta_type m_table_delta;
};

template <typename Dish, typename Hash>
MPIPYP<Dish,Hash>::MPIPYP(double a, double b, Hash h)
: PYP<Dish,Hash>(a, b, 0, h) {}

template <typename Dish, typename Hash>
  template <typename Uniform01>
int 
MPIPYP<Dish,Hash>::increment(Dish dish, double p0, Uniform01& rnd) {
  //std::cerr << "-----INCREMENT DISH " << dish << std::endl;
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
  if (pshare < 0.0) {
    std::cerr << pshare << " " << c << " " << a << " " << t << std::endl;
    assert(false);
  }

  if (rnd() < pnew / (pshare + pnew)) {
    // assign to a new table
    tc.tables += 1;
    tc.table_histogram[1] += 1;
    PYP<Dish,Hash>::_total_tables += 1;
    delta = 1;
    table_joined = 1;
  }
  else {
    // randomly assign to an existing table
    // remove constant denominator from inner loop
    double r = rnd() * (c - a*t);
    for (std::map<int,int>::iterator
         hit = tc.table_histogram.begin();
         hit != tc.table_histogram.end(); ++hit) {
      r -= ((hit->first - a) * hit->second);
      if (r <= 0) {
        tc.table_histogram[hit->first+1] += 1;
        hit->second -= 1;
        table_joined = hit->first+1;
        if (hit->second == 0)
          tc.table_histogram.erase(hit);
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
  /*
  typename PYP<Dish,Hash>::TableCounter &delta_tc = m_table_delta[dish];

  std::map<int,int> &histogram = delta_tc.table_histogram;
  assert (table_joined > 0);

  typename std::map<int,int>::iterator table_it; bool table_insert_result; 
  boost::tie(table_it, table_insert_result) = histogram.insert(std::make_pair(table_joined,0)); 
  table_it->second += 1;
  if (delta == 0) {
    // decrement the histogram bar for the table left 
    typename std::map<int,int>::iterator left_table_it; 
    boost::tie(left_table_it, table_insert_result) 
      = histogram.insert(std::make_pair(table_joined-1,0)); 
    left_table_it->second -= 1;
    if (left_table_it->second == 0) histogram.erase(left_table_it);
  }
  else delta_tc.tables += 1;

  if (table_it->second == 0) histogram.erase(table_it);

    //std::cerr << "Added (" << delta << ") " << dish << " to table " << table_joined << "\n"; 
    //std::cerr << "Dish " << dish << " has " << count(dish) << " customers, and is sitting at " << PYP<Dish,Hash>::num_tables(dish) << " tables.\n"; 
    //for (std::map<int,int>::const_iterator 
    //     hit = delta_tc.table_histogram.begin();
    //     hit != delta_tc.table_histogram.end(); ++hit) {
    //  std::cerr << "    " << hit->second << " tables with " << hit->first << " customers." << std::endl; 
    //}
    //std::cerr << "Added (" << delta << ") " << dish << " to table " << table_joined << "\n"; 
    //std::cerr << "Dish " << dish << " has " << count(dish) << " customers, and is sitting at " << PYP<Dish,Hash>::num_tables(dish) << " tables.\n"; 
    int x_num_customers=0, x_num_table=0;
    for (std::map<int,int>::const_iterator 
         hit = delta_tc.table_histogram.begin();
         hit != delta_tc.table_histogram.end(); ++hit) {
      x_num_table += hit->second;
      x_num_customers += (hit->second*hit->first);
    }
    int tmp_c = PYP<Dish,Hash>::count(dish);
    int tmp_t = PYP<Dish,Hash>::num_tables(dish);
    assert (x_num_customers <= tmp_c); 
    assert (x_num_table <= tmp_t); 

  if (delta_tc.table_histogram.empty()) {
    assert (delta_tc.tables == 0);
    m_table_delta.erase(dish);
  }
  */

  //PYP<Dish,Hash>::debug_info(std::cerr);
  //std::cerr << "   Dish " << dish << " has count " << PYP<Dish,Hash>::count(dish) << " tables " << PYP<Dish,Hash>::num_tables(dish) << std::endl;

  return delta;
}

template <typename Dish, typename Hash>
  template <typename Uniform01>
int 
MPIPYP<Dish,Hash>::decrement(Dish dish, Uniform01& rnd)
{
  //std::cerr << "-----DECREMENT DISH " << dish << std::endl;
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

  double r = rnd() * PYP<Dish,Hash>::count(dish);
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

  // MPI Delta processing
  typename dish_delta_type::iterator it; 
  bool insert_result; 
  boost::tie(it, insert_result) = m_count_delta.insert(std::make_pair(dish,0)); 
  it->second -= 1;
  if (it->second == 0) m_count_delta.erase(it);

  assert (table_left > 0);
  typename PYP<Dish,Hash>::TableCounter& delta_tc = m_table_delta[dish];
  if (table_left > 1) {
    std::map<int,int>::iterator tit;
    boost::tie(tit, insert_result) = delta_tc.table_histogram.insert(std::make_pair(table_left-1,0));
    tit->second += 1;
    if (tit->second == 0) delta_tc.table_histogram.erase(tit);
  }
  else delta_tc.tables -= 1;

  std::map<int,int>::iterator tit;
  boost::tie(tit, insert_result) = delta_tc.table_histogram.insert(std::make_pair(table_left,0));
  tit->second -= 1;
  if (tit->second == 0) delta_tc.table_histogram.erase(tit);

  //  std::cerr << "Dish " << dish << " has " << count(dish) << " customers, and is sitting at " << PYP<Dish,Hash>::num_tables(dish) << " tables.\n"; 
  //  for (std::map<int,int>::const_iterator 
  //       hit = delta_tc.table_histogram.begin();
  //       hit != delta_tc.table_histogram.end(); ++hit) {
  //    std::cerr << "    " << hit->second << " tables with " << hit->first << " customers." << std::endl; 
  //  }
    int x_num_customers=0, x_num_table=0;
    for (std::map<int,int>::const_iterator 
         hit = delta_tc.table_histogram.begin();
         hit != delta_tc.table_histogram.end(); ++hit) {
      x_num_table += hit->second;
      x_num_customers += (hit->second*hit->first);
    }
    int tmp_c = PYP<Dish,Hash>::count(dish);
    int tmp_t = PYP<Dish,Hash>::num_tables(dish);
    assert (x_num_customers <= tmp_c); 
    assert (x_num_table <= tmp_t); 

  if (delta_tc.table_histogram.empty()) {
  //  std::cerr << "   DELETING " << dish << std::endl;
    assert (delta_tc.tables == 0);
    m_table_delta.erase(dish);
  }

  //PYP<Dish,Hash>::debug_info(std::cerr);
  //std::cerr << "   Dish " << dish << " has count " << PYP<Dish,Hash>::count(dish) << " tables " << PYP<Dish,Hash>::num_tables(dish) << std::endl;
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

template <typename Dish>
struct subtract_maps {
  typedef std::map<Dish,int> map_type;
  map_type& operator() (map_type& l, map_type const & r) const {
    for (typename map_type::const_iterator it=r.begin(); it != r.end(); it++)
      l[it->first] -= it->second;
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

template <typename A, typename B, typename C>
struct triple {
  triple() {}
  triple(const A& a, const B& b, const C& c) : first(a), second(b), third(c) {}
  A first;
  B second;
  C third;

  template<class Archive>
  void serialize(Archive &ar, const unsigned int version){
      ar & first;
      ar & second;
      ar & third;
  }
};

BOOST_IS_BITWISE_SERIALIZABLE(MPIPYP<int>::dish_delta_type)
BOOST_CLASS_TRACKING(MPIPYP<int>::dish_delta_type,track_never)

template <typename Dish, typename Hash>
void 
MPIPYP<Dish,Hash>::synchronise(dish_delta_type* result) {
  boost::mpi::communicator world; 
  //int rank = world.rank(), size = world.size();

  boost::mpi::all_reduce(world, m_count_delta, *result, sum_maps<Dish>());
  subtract_maps<Dish>()(*result, m_count_delta);
 
/*
  // communicate the customer count deltas
  dish_delta_type global_dish_delta;
  boost::mpi::all_reduce(world, m_count_delta, global_dish_delta, sum_maps<Dish>());

  // update this restaurant
  for (typename dish_delta_type::const_iterator it=global_dish_delta.begin(); 
       it != global_dish_delta.end(); ++it) {
    int global_delta = it->second - m_count_delta[it->first];
    if (global_delta == 0) continue;
    typename std::tr1::unordered_map<Dish,int,Hash>::iterator dit; bool inserted;
    boost::tie(dit, inserted) 
      = std::tr1::unordered_map<Dish,int,Hash>::insert(std::make_pair(it->first, 0));
    dit->second += global_delta;
    assert(dit->second >= 0);
    if (dit->second == 0) {
      std::tr1::unordered_map<Dish,int,Hash>::erase(dit);
    }

    PYP<Dish,Hash>::_total_customers += (it->second - m_count_delta[it->first]);
    int tmp = PYP<Dish,Hash>::_total_customers;
    assert(tmp >= 0);
    //std::cerr << "Process " << rank << " adding " <<  (it->second - m_count_delta[it->first]) << " of customer " << it->first << std::endl;
  }
*/
/*
  // communicate the table count deltas
  for (int process = 0; process < size; ++process) {
    typename std::vector< triple<Dish, int, int> > message;
    if (rank == process) {
      // broadcast deltas
      for (typename table_delta_type::const_iterator dish_it=m_table_delta.begin(); 
           dish_it != m_table_delta.end(); ++dish_it) {
        //assert (dish_it->second.tables > 0);
        for (std::map<int,int>::const_iterator it=dish_it->second.table_histogram.begin(); 
             it != dish_it->second.table_histogram.end(); ++it) {
          triple<Dish, int, int> m(dish_it->first, it->first, it->second);
          message.push_back(m);
        }
        // append a special message with the total table delta for this dish
        triple<Dish, int, int> m(dish_it->first, -1, dish_it->second.tables);
        message.push_back(m);
      }
      boost::mpi::broadcast(world, message, process);
    }
    else {
      // receive deltas
      boost::mpi::broadcast(world, message, process);
      for (typename std::vector< triple<Dish, int, int> >::const_iterator it=message.begin(); it != message.end(); ++it) {
        typename PYP<Dish,Hash>::TableCounter& tc = PYP<Dish,Hash>::_dish_tables[it->first];
        if (it->second >= 0) {
          std::map<int,int>::iterator tit; bool inserted;
          boost::tie(tit, inserted) = tc.table_histogram.insert(std::make_pair(it->second, 0));
          tit->second += it->third;
          if (tit->second < 0) {
            std::cerr << tit->first << " " << tit->second << " " << it->first << " " << it->second << " " << it->third << std::endl;
            assert(tit->second >= 0);
          }
          if (tit->second == 0) {
            tc.table_histogram.erase(tit);
          }
        }
        else {
          tc.tables += it->third;
          PYP<Dish,Hash>::_total_tables += it->third;
          assert(tc.tables >= 0);
          if (tc.tables == 0) assert(tc.table_histogram.empty());
          if (tc.table_histogram.empty()) {
            assert (tc.tables == 0);
            PYP<Dish,Hash>::_dish_tables.erase(it->first);
          }
        }
      }
    }
  }
*/

//  reset_deltas();
}

#endif
