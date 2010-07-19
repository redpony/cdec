#ifndef _pyp_hh
#define _pyp_hh

#include "slice-sampler.h"
#include <math.h>
#include <map>
#include <tr1/unordered_map>
//#include <google/sparse_hash_map>

#include <boost/random/uniform_real.hpp>
#include <boost/random/variate_generator.hpp>
#include <boost/random/mersenne_twister.hpp>

#include "log_add.h"
#include "mt19937ar.h"

//
// Pitman-Yor process with customer and table tracking
//

template <typename Dish, typename Hash=std::tr1::hash<Dish> >
class PYP : protected std::tr1::unordered_map<Dish, int, Hash>
//class PYP : protected google::sparse_hash_map<Dish, int, Hash>
{
public:
  using std::tr1::unordered_map<Dish,int>::const_iterator;
  using std::tr1::unordered_map<Dish,int>::iterator;
  using std::tr1::unordered_map<Dish,int>::begin;
  using std::tr1::unordered_map<Dish,int>::end;
//  using google::sparse_hash_map<Dish,int>::const_iterator;
//  using google::sparse_hash_map<Dish,int>::iterator;
//  using google::sparse_hash_map<Dish,int>::begin;
//  using google::sparse_hash_map<Dish,int>::end;

  PYP(double a, double b, unsigned long seed = 0, Hash hash=Hash());

  virtual int increment(Dish d, double p0);
  virtual int decrement(Dish d);

  // lookup functions
  int count(Dish d) const;
  double prob(Dish dish, double p0) const;
  double prob(Dish dish, double dcd, double dca, 
              double dtd, double dta, double p0) const;
  double unnormalised_prob(Dish dish, double p0) const;

  int num_customers() const { return _total_customers; }
  int num_types() const { return std::tr1::unordered_map<Dish,int>::size(); }
  //int num_types() const { return google::sparse_hash_map<Dish,int>::size(); }
  bool empty() const { return _total_customers == 0; }

  double log_prob(Dish dish, double log_p0) const;
  // nb. d* are NOT logs
  double log_prob(Dish dish, double dcd, double dca, 
                       double dtd, double dta, double log_p0) const;

  int num_tables(Dish dish) const;
  int num_tables() const;

  double a() const { return _a; }
  void set_a(double a) { _a = a; }

  double b() const { return _b; }
  void set_b(double b) { _b = b; }

  virtual void clear();
  std::ostream& debug_info(std::ostream& os) const;

  double log_restaurant_prob() const;
  double log_prior() const;
  static double log_prior_a(double a, double beta_a, double beta_b);
  static double log_prior_b(double b, double gamma_c, double gamma_s);

  template <typename Uniform01>
    void resample_prior(Uniform01& rnd);
  template <typename Uniform01>
    void resample_prior_a(Uniform01& rnd);
  template <typename Uniform01>
    void resample_prior_b(Uniform01& rnd);

protected:
  double _a, _b; // parameters of the Pitman-Yor distribution
  double _a_beta_a, _a_beta_b; // parameters of Beta prior on a
  double _b_gamma_s, _b_gamma_c; // parameters of Gamma prior on b

  struct TableCounter {
    TableCounter() : tables(0) {};
    int tables;
    std::map<int, int> table_histogram; // num customers at table -> number tables
  };
  typedef std::tr1::unordered_map<Dish, TableCounter, Hash> DishTableType;
  //typedef google::sparse_hash_map<Dish, TableCounter, Hash> DishTableType;
  DishTableType _dish_tables;
  int _total_customers, _total_tables;

  typedef boost::mt19937 base_generator_type;
  typedef boost::uniform_real<> uni_dist_type;
  typedef boost::variate_generator<base_generator_type&, uni_dist_type> gen_type;

//  uni_dist_type uni_dist;
//  base_generator_type rng; //this gets the seed
//  gen_type rnd; //instantiate: rnd(rng, uni_dist)
                //call: rnd() generates uniform on [0,1)

  // Function objects for calculating the parts of the log_prob for 
  // the parameters a and b
  struct resample_a_type {
    int n, m; double b, a_beta_a, a_beta_b;
    const DishTableType& dish_tables;
    resample_a_type(int n, int m, double b, double a_beta_a, 
                    double a_beta_b, const DishTableType& dish_tables)
      : n(n), m(m), b(b), a_beta_a(a_beta_a), a_beta_b(a_beta_b), dish_tables(dish_tables) {}

    double operator() (double proposed_a) const {
      double log_prior = log_prior_a(proposed_a, a_beta_a, a_beta_b);
      double log_prob = 0.0;
      double lgamma1a = lgamma(1.0 - proposed_a);
      for (typename DishTableType::const_iterator dish_it=dish_tables.begin(); dish_it != dish_tables.end(); ++dish_it) 
        for (std::map<int, int>::const_iterator table_it=dish_it->second.table_histogram.begin(); 
             table_it !=dish_it->second.table_histogram.end(); ++table_it) 
          log_prob += (table_it->second * (lgamma(table_it->first - proposed_a) - lgamma1a));

      log_prob += (proposed_a == 0.0 ? (m-1.0)*log(b) 
                   : ((m-1.0)*log(proposed_a) + lgamma((m-1.0) + b/proposed_a) - lgamma(b/proposed_a)));
      assert(std::isfinite(log_prob));
      return log_prob + log_prior;
    }
  };

  struct resample_b_type {
    int n, m; double a, b_gamma_c, b_gamma_s;
    resample_b_type(int n, int m, double a, double b_gamma_c, double b_gamma_s)
      : n(n), m(m), a(a), b_gamma_c(b_gamma_c), b_gamma_s(b_gamma_s) {}

    double operator() (double proposed_b) const {
      double log_prior = log_prior_b(proposed_b, b_gamma_c, b_gamma_s);
      double log_prob = 0.0;
      log_prob += (a == 0.0  ? (m-1.0)*log(proposed_b) 
                  : ((m-1.0)*log(a) + lgamma((m-1.0) + proposed_b/a) - lgamma(proposed_b/a)));
      log_prob += (lgamma(1.0+proposed_b) - lgamma(n+proposed_b));
      return log_prob + log_prior;
    }
  };
   
  /* lbetadist() returns the log probability density of x under a Beta(alpha,beta)
   * distribution. - copied from Mark Johnson's gammadist.c
   */
  static long double lbetadist(long double x, long double alpha, long double beta);

  /* lgammadist() returns the log probability density of x under a Gamma(alpha,beta)
   * distribution - copied from Mark Johnson's gammadist.c
   */
  static long double lgammadist(long double x, long double alpha, long double beta);

};

template <typename Dish, typename Hash>
PYP<Dish,Hash>::PYP(double a, double b, unsigned long seed, Hash)
: std::tr1::unordered_map<Dish, int, Hash>(10), _a(a), _b(b), 
//: google::sparse_hash_map<Dish, int, Hash>(10), _a(a), _b(b), 
  _a_beta_a(1), _a_beta_b(1), _b_gamma_s(1), _b_gamma_c(1),
  //_a_beta_a(1), _a_beta_b(1), _b_gamma_s(10), _b_gamma_c(0.1),
  _total_customers(0), _total_tables(0)//,
  //uni_dist(0,1), rng(seed == 0 ? (unsigned long)this : seed), rnd(rng, uni_dist)
{
//  std::cerr << "\t##PYP<Dish,Hash>::PYP(a=" << _a << ",b=" << _b << ")" << std::endl;
  //set_deleted_key(-std::numeric_limits<Dish>::max());
}

template <typename Dish, typename Hash>
double 
PYP<Dish,Hash>::prob(Dish dish, double p0) const
{
  int c = count(dish), t = num_tables(dish);
  double r = num_tables() * _a + _b;
  //std::cerr << "\t\t\t\tPYP<Dish,Hash>::prob(" << dish << "," << p0 << ") c=" << c << " r=" << r << std::endl;
  if (c > 0)
    return (c - _a * t + r * p0) / (num_customers() + _b);
  else
    return r * p0 / (num_customers() + _b);
}

template <typename Dish, typename Hash>
double 
PYP<Dish,Hash>::unnormalised_prob(Dish dish, double p0) const
{
  int c = count(dish), t = num_tables(dish);
  double r = num_tables() * _a + _b;
  if (c > 0) return (c - _a * t + r * p0);
  else       return r * p0;
}

template <typename Dish, typename Hash>
double 
PYP<Dish,Hash>::prob(Dish dish, double dcd, double dca, 
                     double dtd, double dta, double p0)
const
{
  int c = count(dish) + dcd, t = num_tables(dish) + dtd;
  double r = (num_tables() + dta) * _a + _b;
  if (c > 0)
    return (c - _a * t + r * p0) / (num_customers() + dca + _b);
  else
    return r * p0 / (num_customers() + dca + _b);
}

template <typename Dish, typename Hash>
double 
PYP<Dish,Hash>::log_prob(Dish dish, double log_p0) const
{
  using std::log;
  int c = count(dish), t = num_tables(dish);
  double r = log(num_tables() * _a + b);
  if (c > 0)
    return Log<double>::add(log(c - _a * t), r + log_p0)
      - log(num_customers() + _b);
  else
    return r + log_p0 - log(num_customers() + b);
}

template <typename Dish, typename Hash>
double 
PYP<Dish,Hash>::log_prob(Dish dish, double dcd, double dca, 
                         double dtd, double dta, double log_p0)
const
{
  using std::log;
  int c = count(dish) + dcd, t = num_tables(dish) + dtd;
  double r = log((num_tables() + dta) * _a + b);
  if (c > 0)
    return Log<double>::add(log(c - _a * t), r + log_p0)
      - log(num_customers() + dca + _b);
  else
    return r + log_p0 - log(num_customers() + dca + b);
}

template <typename Dish, typename Hash>
int 
PYP<Dish,Hash>::increment(Dish dish, double p0) {
  int delta = 0;
  TableCounter &tc = _dish_tables[dish];

  // seated on a new or existing table?
  int c = count(dish), t = num_tables(dish), T = num_tables();
  double pshare = (c > 0) ? (c - _a*t) : 0.0;
  double pnew = (_b + _a*T) * p0;
  assert (pshare >= 0.0);
  //assert (pnew > 0.0);

  //if (rnd() < pnew / (pshare + pnew)) {
  if (mt_genrand_res53() < pnew / (pshare + pnew)) {
    // assign to a new table
    tc.tables += 1;
    tc.table_histogram[1] += 1;
    _total_tables += 1;
    delta = 1;
  }
  else {
    // randomly assign to an existing table
    // remove constant denominator from inner loop
    //double r = rnd() * (c - _a*t);
    double r = mt_genrand_res53() * (c - _a*t);
    for (std::map<int,int>::iterator
         hit = tc.table_histogram.begin();
         hit != tc.table_histogram.end(); ++hit) {
      r -= ((hit->first - _a) * hit->second);
      if (r <= 0) {
        tc.table_histogram[hit->first+1] += 1;
        hit->second -= 1;
        if (hit->second == 0)
          tc.table_histogram.erase(hit);
        break;
      }
    }
    if (r > 0) {
      std::cerr << r << " " << c << " " << _a << " " << t << std::endl;
      assert(false);
    }
    delta = 0;
  }

  std::tr1::unordered_map<Dish,int,Hash>::operator[](dish) += 1;
  //google::sparse_hash_map<Dish,int,Hash>::operator[](dish) += 1;
  _total_customers += 1;

  return delta;
}

template <typename Dish, typename Hash>
int 
PYP<Dish,Hash>::count(Dish dish) const
{
  typename std::tr1::unordered_map<Dish, int>::const_iterator 
  //typename google::sparse_hash_map<Dish, int>::const_iterator 
    dcit = find(dish);
  if (dcit != end())
    return dcit->second;
  else
    return 0;
}

template <typename Dish, typename Hash>
int 
PYP<Dish,Hash>::decrement(Dish dish)
{
  typename std::tr1::unordered_map<Dish, int>::iterator dcit = find(dish);
  //typename google::sparse_hash_map<Dish, int>::iterator dcit = find(dish);
  if (dcit == end()) {
    std::cerr << dish << std::endl;
    assert(false);
  } 

  int delta = 0;

  typename std::tr1::unordered_map<Dish, TableCounter>::iterator dtit = _dish_tables.find(dish);
  //typename google::sparse_hash_map<Dish, TableCounter>::iterator dtit = _dish_tables.find(dish);
  if (dtit == _dish_tables.end()) {
    std::cerr << dish << std::endl;
    assert(false);
  } 
  TableCounter &tc = dtit->second;

  //std::cerr << "\tdecrement for " << dish << "\n";
  //std::cerr << "\tBEFORE histogram: " << tc.table_histogram << " ";
  //std::cerr << "count: " << count(dish) << " ";
  //std::cerr << "tables: " << tc.tables << "\n";

  //double r = rnd() * count(dish);
  double r = mt_genrand_res53() * count(dish);
  for (std::map<int,int>::iterator hit = tc.table_histogram.begin();
       hit != tc.table_histogram.end(); ++hit)
  {
    //r -= (hit->first - _a) * hit->second;
    r -= (hit->first) * hit->second;
    if (r <= 0)
    {
      if (hit->first > 1)
        tc.table_histogram[hit->first-1] += 1;
      else
      {
        delta = -1;
        tc.tables -= 1;
        _total_tables -= 1;
      }

      hit->second -= 1;
      if (hit->second == 0) tc.table_histogram.erase(hit);
      break;
    }
  }
  if (r > 0) {
    std::cerr << r << " " << count(dish) << " " << _a << " " << num_tables(dish) << std::endl;
    assert(false);
  }

  // remove the customer
  dcit->second -= 1;
  _total_customers -= 1;
  assert(dcit->second >= 0);
  if (dcit->second == 0) {
    erase(dcit);
    _dish_tables.erase(dtit);
    //std::cerr << "\tAFTER histogram: Empty\n";
  }
  else {
    //std::cerr << "\tAFTER histogram: " << _dish_tables[dish].table_histogram << " ";
    //std::cerr << "count: " << count(dish) << " ";
    //std::cerr << "tables: " << _dish_tables[dish].tables << "\n";
  }

  return delta;
}

template <typename Dish, typename Hash>
int 
PYP<Dish,Hash>::num_tables(Dish dish) const
{
  typename std::tr1::unordered_map<Dish, TableCounter, Hash>::const_iterator 
  //typename google::sparse_hash_map<Dish, TableCounter, Hash>::const_iterator 
    dtit = _dish_tables.find(dish);

  //assert(dtit != _dish_tables.end());
  if (dtit == _dish_tables.end())
    return 0;

  return dtit->second.tables;
}

template <typename Dish, typename Hash>
int 
PYP<Dish,Hash>::num_tables() const
{
  return _total_tables;
}

template <typename Dish, typename Hash>
std::ostream&
PYP<Dish,Hash>::debug_info(std::ostream& os) const
{
  int hists = 0, tables = 0;
  for (typename std::tr1::unordered_map<Dish, TableCounter, Hash>::const_iterator 
  //for (typename google::sparse_hash_map<Dish, TableCounter, Hash>::const_iterator 
       dtit = _dish_tables.begin(); dtit != _dish_tables.end(); ++dtit)
  {
    hists += dtit->second.table_histogram.size();
    tables += dtit->second.tables;

//    if (dtit->second.tables <= 0)
//      std::cerr << dtit->first << " " << count(dtit->first) << std::endl;
    assert(dtit->second.tables > 0);
    assert(!dtit->second.table_histogram.empty());

//    os << "Dish " << dtit->first << " has " << count(dtit->first) << " customers, and is sitting at " << dtit->second.tables << " tables.\n"; 
    for (std::map<int,int>::const_iterator 
         hit = dtit->second.table_histogram.begin();
         hit != dtit->second.table_histogram.end(); ++hit) {
//      os << "    " << hit->second << " tables with " << hit->first << " customers." << std::endl; 
      assert(hit->second > 0);
    }
  }

  os << "restaurant has " 
    << _total_customers << " customers; "
    << _total_tables << " tables; " 
    << tables << " tables'; " 
    << num_types() << " dishes; "
    << _dish_tables.size() << " dishes'; and "
    << hists << " histogram entries\n";

  return os;
}

template <typename Dish, typename Hash>
void 
PYP<Dish,Hash>::clear()
{
  this->std::tr1::unordered_map<Dish,int,Hash>::clear();
  //this->google::sparse_hash_map<Dish,int,Hash>::clear();
  _dish_tables.clear();
  _total_tables = _total_customers = 0;
}

// log_restaurant_prob returns the log probability of the PYP table configuration.
// Excludes Hierarchical P0 term which must be calculated separately.
template <typename Dish, typename Hash>
double 
PYP<Dish,Hash>::log_restaurant_prob() const {
  if (_total_customers < 1)
    return (double)0.0;

  double log_prob = 0.0;
  double lgamma1a = lgamma(1.0-_a);

  //std::cerr << "-------------------\n" << std::endl;
  for (typename DishTableType::const_iterator dish_it=_dish_tables.begin(); 
       dish_it != _dish_tables.end(); ++dish_it) {
    for (std::map<int, int>::const_iterator table_it=dish_it->second.table_histogram.begin(); 
         table_it !=dish_it->second.table_histogram.end(); ++table_it) {
      log_prob += (table_it->second * (lgamma(table_it->first - _a) - lgamma1a));
      //std::cerr << "|" << dish_it->first->parent << " --> " << dish_it->first->rhs << " " << table_it->first << " " << table_it->second << " " << log_prob;
    }
  }
  //std::cerr << std::endl;

  log_prob += (_a == (double)0.0 ? (_total_tables-1.0)*log(_b) : (_total_tables-1.0)*log(_a) + lgamma((_total_tables-1.0) + _b/_a) - lgamma(_b/_a));
  //std::cerr << "\t\t" << log_prob << std::endl;
  log_prob += (lgamma(1.0 + _b) - lgamma(_total_customers + _b));

  //std::cerr << _total_customers << " " << _total_tables << " " << log_prob << " " << log_prior() << std::endl;
  //std::cerr << _a << " " << _b << std::endl;
  if (!std::isfinite(log_prob)) {
    assert(false);
  }
  //return log_prob;
  return log_prob + log_prior();
}

template <typename Dish, typename Hash>
double 
PYP<Dish,Hash>::log_prior() const {
  double prior = 0.0;
  if (_a_beta_a > 0.0 && _a_beta_b > 0.0 && _a > 0.0)
    prior += log_prior_a(_a, _a_beta_a, _a_beta_b);
  if (_b_gamma_s > 0.0 && _b_gamma_c > 0.0)
    prior += log_prior_b(_b, _b_gamma_c, _b_gamma_s);

  return prior;
}

template <typename Dish, typename Hash>
double 
PYP<Dish,Hash>::log_prior_a(double a, double beta_a, double beta_b) {
  return lbetadist(a, beta_a, beta_b); 
}

template <typename Dish, typename Hash>
double 
PYP<Dish,Hash>::log_prior_b(double b, double gamma_c, double gamma_s) {
  return lgammadist(b, gamma_c, gamma_s); 
}

template <typename Dish, typename Hash>
long double PYP<Dish,Hash>::lbetadist(long double x, long double alpha, long double beta) {
  assert(x > 0);
  assert(x < 1);
  assert(alpha > 0);
  assert(beta > 0);
  return (alpha-1)*log(x)+(beta-1)*log(1-x)+lgamma(alpha+beta)-lgamma(alpha)-lgamma(beta);
//boost::math::lgamma
}

template <typename Dish, typename Hash>
long double PYP<Dish,Hash>::lgammadist(long double x, long double alpha, long double beta) {
  assert(alpha > 0);
  assert(beta > 0);
  return (alpha-1)*log(x) - alpha*log(beta) - x/beta - lgamma(alpha);
}


template <typename Dish, typename Hash>
  template <typename Uniform01>
void 
PYP<Dish,Hash>::resample_prior(Uniform01& rnd) {
  for (int num_its=5; num_its >= 0; --num_its) {
    resample_prior_b(rnd);
    resample_prior_a(rnd);
  }
  resample_prior_b(rnd);
}

template <typename Dish, typename Hash>
  template <typename Uniform01>
void 
PYP<Dish,Hash>::resample_prior_b(Uniform01& rnd) {
  if (_total_tables == 0) 
    return;

  //int niterations = 10;   // number of resampling iterations
  int niterations = 5;   // number of resampling iterations
  //std::cerr << "\n## resample_prior_b(), initial a = " << _a << ", b = " << _b << std::endl;
  resample_b_type b_log_prob(_total_customers, _total_tables, _a, _b_gamma_c, _b_gamma_s);
  _b = slice_sampler1d(b_log_prob, _b, rnd, (double) 0.0, std::numeric_limits<double>::infinity(), 
  //_b = slice_sampler1d(b_log_prob, _b, mt_genrand_res53, (double) 0.0, std::numeric_limits<double>::infinity(), 
                       (double) 0.0, niterations, 100*niterations);
  //std::cerr << "\n## resample_prior_b(), final a = " << _a << ", b = " << _b << std::endl;
}

template <typename Dish, typename Hash>
  template <typename Uniform01>
void 
PYP<Dish,Hash>::resample_prior_a(Uniform01& rnd) {
  if (_total_tables == 0) 
    return;

  //int niterations = 10;
  int niterations = 5;
  //std::cerr << "\n## Initial a = " << _a << ", b = " << _b << std::endl;
  resample_a_type a_log_prob(_total_customers, _total_tables, _b, _a_beta_a, _a_beta_b, _dish_tables);
  _a = slice_sampler1d(a_log_prob, _a, rnd, std::numeric_limits<double>::min(), 
  //_a = slice_sampler1d(a_log_prob, _a, mt_genrand_res53, std::numeric_limits<double>::min(), 
                       (double) 1.0, (double) 0.0, niterations, 100*niterations);
}

#endif
