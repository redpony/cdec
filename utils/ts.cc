#include <iostream>
#include <map>
#include <cassert>

#include <boost/static_assert.hpp>

#include "prob.h"
#include "sparse_vector.h"
#include "fast_sparse_vector.h"

using namespace std;

template <typename T>
void Print(const T& x) {
  typename T::const_iterator it = x.begin();
  for (; it != x.end(); ++it) {
    cerr << it->first << ":" << it->second << " ";
  }
  cerr << endl;
}

template <typename T>
void test_unique() {
  T x;
  x.set_value(100, 1.0);
  x.set_value(100, 2.0);
  Print(x);
  assert(x.size() == 1);
  assert(x.value(100) == 2.0);
}

int main() {
  cerr << sizeof(prob_t) << " " << sizeof(LogVal<float>) << endl;
  cerr << " sizeof(FSV<float>) = " << sizeof(FastSparseVector<float>) << endl;
  cerr << "sizeof(FSV<double>) = " << sizeof(FastSparseVector<double>) << endl;
  test_unique<FastSparseVector<float> >();
//  sranddev();
  int c = 0;
  FastSparseVector<float> p;
  for (int i = 0; i < 1000000; ++i) {
    FastSparseVector<float> x;
    for (int j = 0; j < 10; ++j) {
      const int k = rand() % 200;
      const float v = 1000.0f / rand();
      x.set_value(k,v);
    }
    if (x.value(50)) { c++; }
    p = x;
    p += p;
    FastSparseVector<float> y = p + p;
    y *= 2;
    y -= y;
  }
  cerr << "Counted " << c << " times\n";
  return 0;
}

