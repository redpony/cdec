#ifndef _SPARSE_VECTOR_H_
#define _SPARSE_VECTOR_H_

#include "fast_sparse_vector.h"
#define SparseVector FastSparseVector

template <class T, typename S>
SparseVector<T> operator*(const SparseVector<T>& a, const S& b) {
  SparseVector<T> result = a;
  return result *= b;
}

template <class T>
SparseVector<T> operator*(const SparseVector<T>& a, const double& b) {
  SparseVector<T> result = a;
  return result *= b;
}

template <class T, typename S>
SparseVector<T> operator/(const SparseVector<T>& a, const S& b) {
  SparseVector<T> result = a;
  return result /= b;
}

template <class T>
SparseVector<T> operator/(const SparseVector<T>& a, const double& b) {
  SparseVector<T> result = a;
  return result /= b;
}

#include "fdict.h"

template <class O, typename T>
inline void print(O &o,const SparseVector<T>& v, const char* kvsep="=",const char* pairsep=" ",const char* pre="",const char* post="") {
  o << pre;
  bool first=true;
  for (typename SparseVector<T>::const_iterator i=v.begin(),e=v.end();i!=e;++i) {
    if (first)
      first=false;
    else
      o<<pairsep;
    o<<FD::Convert(i->first)<<kvsep<<i->second;
  }
  o << post;
}

template <typename T>
inline std::ostream& operator<<(std::ostream& out, const SparseVector<T>& v) {
  print(out, v);
  return out;
}

namespace B64 {
  void Encode(double objective, const SparseVector<double>& v, std::ostream* out);
  // returns false if failed to decode
  bool Decode(double* objective, SparseVector<double>* v, const char* data, size_t size);
}

#endif
