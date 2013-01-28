#include "veb.h"

#include "veb_bitset.h"
#include "veb_tree.h"

int VEB::MIN_BOTTOM_BITS = 5;
int VEB::MIN_BOTTOM_SIZE = 1 << VEB::MIN_BOTTOM_BITS;

shared_ptr<VEB> VEB::Create(int size) {
  if (size > MIN_BOTTOM_SIZE) {
    return shared_ptr<VEB>(new VEBTree(size));
  } else {
    return shared_ptr<VEB>(new VEBBitset(size));
  }
}

int VEB::GetMinimum() {
  return min;
}

int VEB::GetMaximum() {
  return max;
}

VEB::VEB(int min, int max) : min(min), max(max) {}
