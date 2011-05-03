#ifndef _SPARSE_VECTOR_H_
#define _SPARSE_VECTOR_H_

#if 0

#if HAVE_BOOST_ARCHIVE_TEXT_OARCHIVE_HPP
  friend class boost::serialization::access;
  template<class Archive>
  void save(Archive & ar, const unsigned int version) const {
    (void) version;
    int eff_size = values_.size();
    const_iterator it = this->begin();
    if (values_.find(0) != values_.end()) { ++it; --eff_size; }
    ar & eff_size;
    while (it != this->end()) {
      const std::pair<const std::string&, const T&> wire_pair(FD::Convert(it->first), it->second);
      ar & wire_pair;
      ++it;
    }
  }
  template<class Archive>
  void load(Archive & ar, const unsigned int version) {
    (void) version;
    this->clear();
    int sz; ar & sz;
    for (int i = 0; i < sz; ++i) {
      std::pair<std::string, T> wire_pair;
      ar & wire_pair;
      this->set_value(FD::Convert(wire_pair.first), wire_pair.second);
    }
  }
  BOOST_SERIALIZATION_SPLIT_MEMBER()
#endif
};

#if HAVE_BOOST_ARCHIVE_TEXT_OARCHIVE_HPP
BOOST_CLASS_TRACKING(SparseVector<double>,track_never)
#endif

#endif /// FIX

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
