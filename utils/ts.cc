#include <iostream>
#include <map>
#include <cassert>

#include <boost/static_assert.hpp>

#include "prob.h"
#include "sparse_vector.h"
#include "fast_sparse_vector.h"
#include "stringlib.h"

using namespace std;

template <typename T>
void MPrint(const T& x) {
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
  MPrint(x);
  assert(x.size() == 1);
  assert(x.value(100) == 2.0);
}

void test_logv() {
  FastSparseVector<prob_t> x;
  cerr << "FSV<prob_t> = " << sizeof(FastSparseVector<prob_t>) << endl;
  x.set_value(999, prob_t(0.5));
  x.set_value(0, prob_t());
  x.set_value(1, prob_t(1));
  MPrint(x);
  x += x;
  MPrint(x);
  x -= x;
  MPrint(x);
  FastSparseVector<prob_t> y;
  y = x;
  for (int i = 1; i < 10; ++i) {
    x.set_value(i, prob_t(i*1.3));
    y.set_value(i*2, prob_t(i*1.4));
  }
  swap(x,y);
  MPrint(y);
  MPrint(x);
  x = y;
  y = y;
  x = x;
  MPrint(y);
}

int main() {
  cerr << sizeof(prob_t) << " " << sizeof(LogVal<float>) << endl;
  cerr << " sizeof(FSV<float>) = " << sizeof(FastSparseVector<float>) << endl;
  cerr << "sizeof(FSV<double>) = " << sizeof(FastSparseVector<double>) << endl;
  test_unique<FastSparseVector<float> >();
  test_logv();
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

  cerr << md5("this is a test") << endl;
  cerr << md5("some other ||| string is") << endl;
  map<string,string> x; x["id"] = "12"; x["grammar"] = "/path/to/grammar.gz";
  cerr << SGMLOpenSegTag(x) << endl;
  return 0;
}

