#include <iostream>
#include <map>
#include <cassert>

#include <boost/static_assert.hpp>

#include "prob.h"
#include "sparse_vector.h"
#include "fast_sparse_vector.h"

using namespace std;

int main() {
  cerr << sizeof(prob_t) << " " << sizeof(LogVal<float>) << endl;
  cerr << " sizeof(FSV<float>) = " << sizeof(FastSparseVector<float>) << endl;
  cerr << "sizeof(FSV<double>) = " << sizeof(FastSparseVector<double>) << endl;
  sranddev();
  int c = 0;
  for (int i = 0; i < 1000000; ++i) {
    FastSparseVector<float> x;
    //SparseVector<float> x;
    for (int j = 0; j < 15; ++j) {
      const int k = rand() % 1000;
      const float v = rand() / 3.14f;
      x.set_value(k,v);
    }
    //SparseVector<float> y = x;
    FastSparseVector<float> y = x;
    y += x;
    y = x;
    if (y.value(50)) { c++; }
  }
  cerr << "Counted " << c << " times\n";
  return 0;
}

