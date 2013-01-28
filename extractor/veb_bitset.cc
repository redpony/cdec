#include "veb_bitset.h"

using namespace std;

VEBBitset::VEBBitset(int size) : bitset(size) {
  min = max = -1;
}

void VEBBitset::Insert(int value) {
  bitset[value] = 1;
  if (min == -1 || value < min) {
    min = value;
  }
  if (max == - 1 || value > max) {
    max = value;
  }
}

int VEBBitset::GetSuccessor(int value) {
  int next_value = bitset.find_next(value);
  if (next_value == bitset.npos) {
    return -1;
  }
  return next_value;
}
