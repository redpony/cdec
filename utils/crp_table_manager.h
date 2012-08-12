#ifndef _CRP_TABLE_MANAGER_H_
#define _CRP_TABLE_MANAGER_H_

#include <iostream>
#include "sparse_vector.h"
#include "sampler.h"

// these are helper classes for implementing token-based CRP samplers
// basically the data structures recommended by Blunsom et al. in the Note.

struct CRPHistogram {
  //typedef std::map<unsigned, unsigned> MAPTYPE;
  typedef SparseVector<unsigned> MAPTYPE;
  typedef MAPTYPE::const_iterator const_iterator;

  inline void increment(unsigned bin, unsigned delta = 1u) {
    data[bin] += delta;
  }
  inline void decrement(unsigned bin, unsigned delta = 1u) {
    unsigned r = data[bin] -= delta;
    if (!r) data.erase(bin);
  }
  inline void move(unsigned from_bin, unsigned to_bin, unsigned delta = 1u) {
    decrement(from_bin, delta);
    increment(to_bin, delta);
  }
  inline const_iterator begin() const { return data.begin(); }
  inline const_iterator end() const { return data.end(); }

 private:
  MAPTYPE data;
};

// A CRPTableManager tracks statistics about all customers
// and tables serving some dish in a CRP and can correctly sample what
// table to remove a customer from and what table to join
struct CRPTableManager {
  CRPTableManager() : customers(), tables() {}

  inline unsigned num_tables() const {
    return tables;
  }

  inline unsigned num_customers() const {
    return customers;
  }

  inline void create_table() {
    h.increment(1);
    ++tables;
    ++customers;
  }

  // seat a customer at a table proportional to the number of customers seated at a table, less the discount
  // *new tables are never created by this function!
  inline void share_table(const double discount, MT19937* rng) {
    const double z = customers - discount * num_tables();
    double r = z * rng->next();
    const CRPHistogram::const_iterator end = h.end();
    CRPHistogram::const_iterator it = h.begin();
    for (; it != end; ++it) {
      // it->first = number of customers at table
      // it->second = number of such tables
      double thresh = (it->first - discount) * it->second;
      if (thresh > r) break;
      r -= thresh;
    }
    h.move(it->first, it->first + 1);
    ++customers;
  }

  // randomly sample a customer
  // *tables may be removed
  // returns -1 if a table is removed, 0 otherwise
  inline int remove_customer(MT19937* rng) {
    int r = rng->next() * num_customers();
    const CRPHistogram::const_iterator end = h.end();
    CRPHistogram::const_iterator it = h.begin();
    for (; it != end; ++it) {
      int thresh = it->first * it->second;
      if (thresh > r) break;
      r -= thresh;
    }
    --customers;
    const unsigned tc = it->first;
    if (tc == 1) {
      h.decrement(1);
      --tables;
      return -1;
    } else {
      h.move(tc, tc - 1);
      return 0;
    }
  }

  unsigned customers;
  unsigned tables;
  CRPHistogram h;
};

std::ostream& operator<<(std::ostream& os, const CRPTableManager& tm) {
  os << '[' << tm.num_customers() << " total customers at " << tm.num_tables() << " tables ||| ";
  for (CRPHistogram::const_iterator it = tm.h.begin(); it != tm.h.end(); ++it) {
    if (it != tm.h.begin()) os << "  --  ";
    os << '(' << it->first << ") x " << it->second;
  }
  return os << ']';
}

#endif
