#ifndef _VEB_BITSET_H_
#define _VEB_BITSET_H_

#include <boost/dynamic_bitset.hpp>

#include "veb.h"

class VEBBitset: public VEB {
 public:
  VEBBitset(int size);

  void Insert(int value);

  int GetMinimum();

  int GetSuccessor(int value);

 private:
  boost::dynamic_bitset<> bitset;
};

#endif
