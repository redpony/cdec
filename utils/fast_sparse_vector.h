#ifndef _FAST_SPARSE_VECTOR_H_
#define _FAST_SPARSE_VECTOR_H_

// FastSparseVector<T> is a integer indexed unordered map that supports very fast
// (mathematical) vector operations when the sizes are very small, and reasonably
// fast operations when the sizes are large.
// important: indexes are integers
// important: iterators may return elements in any order

#include <cstring>
#include <climits>
#include <map>
#include <cassert>
#include <vector>

#include <boost/static_assert.hpp>

// this is architecture dependent, it should be
// detected in some way but it's probably easiest (for me)
// to just set it
#define L2_CACHE_LINE 128

// this should just be a typedef to pair<int,T> on the new c++
template <typename T>
struct PairIntT {
  int first;
  T second;
  const PairIntT& operator=(const std::pair<const int, T>& v) {
    first = v.first;
    second = v.second;
    return *this;
  }
  operator const std::pair<const int, T>&() const {
    return *reinterpret_cast<const std::pair<const int, T>*>(this);
  }
};
BOOST_STATIC_ASSERT(sizeof(PairIntT<float>) == sizeof(std::pair<int,float>));

template <typename T, int LOCAL_MAX = (sizeof(T) == sizeof(float) ? 15 : 7)>
class FastSparseVector {
 public:
  struct const_iterator {
    const_iterator(const FastSparseVector<T>& v, const bool is_end) : local_(v.is_local_) {
      if (local_) {
        local_it_ = &v.data_.local[is_end ? v.local_size_ : 0];
      } else {
        if (is_end)
          remote_it_ = v.data_.rbmap->end();
        else
          remote_it_ = v.data_.rbmap->begin();
      }
    }
    const bool local_;
    const PairIntT<T>* local_it_;
    typename std::map<int, T>::const_iterator remote_it_;
    const std::pair<const int, T>& operator*() const {
      if (local_)
        return *reinterpret_cast<const std::pair<const int, float>*>(local_it_);
      else
        return *remote_it_;
    }

    const std::pair<const int, T>* operator->() const {
      if (local_)
        return reinterpret_cast<const std::pair<const int, T>*>(local_it_);
      else
        return &*remote_it_;
    }

    const_iterator& operator++() {
      if (local_) ++local_it_; else ++remote_it_;
      return *this;
    }

    inline bool operator==(const const_iterator& o) const {
      if (o.local_ != local_) return false;
      if (local_) {
        return local_it_ == o.local_it_;
      } else {
        return remote_it_ == o.remote_it_;
      }
    }
    inline bool operator!=(const const_iterator& o) const {
      return !(o == *this);
    }
  };
 public:
  FastSparseVector() : local_size_(0), is_local_(true) {}
  ~FastSparseVector() {
    if (!is_local_) delete data_.rbmap;
  }
  FastSparseVector(const FastSparseVector& other) {
    std::memcpy(this, &other, sizeof(FastSparseVector));
    if (is_local_) return;
    data_.rbmap = new std::map<int, T>(*data_.rbmap);
  }
  const FastSparseVector& operator=(const FastSparseVector& other) {
    if (!is_local_) delete data_.rbmap;
    std::memcpy(this, &other, sizeof(FastSparseVector));
    if (is_local_) return *this;
    data_.rbmap = new std::map<int, T>(*data_.rbmap);
    return *this;
  }
  inline void set_value(int k, const T& v) {
    get_or_create_bin(k) = v;
  }
  inline T value(int k) const {
    if (is_local_) {
      for (int i = 0; i < local_size_; ++i) {
        const PairIntT<T>& p = data_.local[i];
        if (p.first == k) return p.second;
      }
    } else {
      typename std::map<int, T>::const_iterator it = data_.rbmap->find(k);
      if (it != data_.rbmap->end()) return it->second;
    }
    return T();
  }
  inline size_t size() const {
    if (is_local_) return local_size_;
    return data_.rbmap->size();
  }
  inline void clear() {
    if (!is_local_) delete data_.rbmap;
    local_size_ = 0;
  }
  inline bool empty() const {
    return size() == 0;
  }
  inline FastSparseVector& operator+=(const FastSparseVector& other) {
    if (empty()) { *this = other; return *this; }
    const typename FastSparseVector::const_iterator end = other.end();
    for (typename FastSparseVector::const_iterator it = other.begin(); it != end; ++it) {
      get_or_create_bin(it->first) += it->second;
    }
    return *this;
  }
  inline FastSparseVector& operator-=(const FastSparseVector& other) {
    const typename FastSparseVector::const_iterator end = other.end();
    for (typename FastSparseVector::const_iterator it = other.begin(); it != end; ++it) {
      get_or_create_bin(it->first) -= it->second;
    }
    return *this;
  }
  inline FastSparseVector& operator*=(const T& scalar) {
    if (is_local_) {
      for (int i = 0; i < local_size_; ++i)
        data_.local[i].second *= scalar;
    } else {
      const typename std::map<int, T>::iterator end = data_.rbmap->end();
      for (typename std::map<int, T>::iterator it = data_.rbmap->begin(); it != end; ++it)
        it->second *= scalar;
    }
    return *this;
  }
  inline FastSparseVector& operator/=(const T& scalar) {
    if (is_local_) {
      for (int i = 0; i < local_size_; ++i)
        data_.local[i].second /= scalar;
    } else {
      const typename std::map<int, T>::iterator end = data_.rbmap->end();
      for (typename std::map<int, T>::iterator it = data_.rbmap->begin(); it != end; ++it)
        it->second /= scalar;
    }
    return *this;
  }
  const_iterator begin() const {
    return const_iterator(*this, false);
  }
  const_iterator end() const {
    return const_iterator(*this, true);
  }
  void init_vector(std::vector<T> *vp) const {
    init_vector(*vp);
  }
  void init_vector(std::vector<T> &v) const {
    v.clear();
    for (const_iterator i=begin(),e=end();i!=e;++i)
      extend_vector(v,i->first)=i->second;
  }
  T dot(const std::vector<T>& v) const {
    T res = T();
    for (const_iterator it = begin(), e = end(); it != e; ++it)
      if (it->first < v.size()) res += it->second * v[it->first];
  }
 private:
  inline T& extend_vector(std::vector<T> &v,int i) {
    if (i>=v.size())
      v.resize(i+1);
    return v[i];
  }
  inline T& get_or_create_bin(int k) {
    if (is_local_) {
      for (int i = 0; i < local_size_; ++i)
        if (data_.local[i].first == k) return data_.local[i].second;
    } else {
      return (*data_.rbmap)[k];
    }
    assert(is_local_);
    // currently local!
    if (local_size_ < LOCAL_MAX) {
      PairIntT<T>& p = data_.local[local_size_];
      ++local_size_;
      p.first = k;
      return p.second;
    } else {
      swap_local_rbmap();
      return (*data_.rbmap)[k];  
    }
  }
  void swap_local_rbmap() {
    if (is_local_) { // data is local, move to rbmap
      std::map<int, T>* m = new std::map<int, T>(&data_.local[0], &data_.local[local_size_]);
      data_.rbmap = m;
      is_local_ = false;
    } else { // data is in rbmap, move to local
      assert(data_.rbmap->size() < LOCAL_MAX);
      const std::map<int, T>* m = data_.rbmap;
      local_size_ = m->size();
      int i = 0;
      for (typename std::map<int, T>::const_iterator it = m->begin();
           it != m->end(); ++it) {
        data_.local[i] = *it;
        ++i;
      }
      is_local_ = true;
    }
  }

  union {
    PairIntT<T> local[LOCAL_MAX];
    std::map<int, T>* rbmap;
  } data_;
  unsigned char local_size_;
  bool is_local_;
};

template <typename T>
const FastSparseVector<T> operator+(const FastSparseVector<T>& x, const FastSparseVector<T>& y) {
  if (x.size() > y.size()) {
    FastSparseVector<T> res(x);
    res += y;
    return res;
  } else {
    FastSparseVector<T> res(y);
    res += x;
    return res;
  }
}

template <typename T>
const FastSparseVector<T> operator-(const FastSparseVector<T>& x, const FastSparseVector<T>& y) {
  if (x.size() > y.size()) {
    FastSparseVector<T> res(x);
    res -= y;
    return res;
  } else {
    FastSparseVector<T> res(y);
    res -= x;
    return res;
  }
}

namespace performance_checks {
  // if you get a failure on the next line, you should adjust LOCAL_MAX for
  // your architecture
  BOOST_STATIC_ASSERT(sizeof(FastSparseVector<float>) == L2_CACHE_LINE);
};

#include "fdict.h"

template <class O, typename T>
inline void print(O &o,const FastSparseVector<T>& v, const char* kvsep="=",const char* pairsep=" ",const char* pre="",const char* post="") {
  o << pre;
  bool first=true;
  for (typename FastSparseVector<T>::const_iterator i=v.begin(),e=v.end();i!=e;++i) {
    if (first)
      first=false;
    else
      o<<pairsep;
    o<<FD::Convert(i->first)<<kvsep<<i->second;
  }
  o << post;
}

template <typename T>
inline std::ostream& operator<<(std::ostream& out, const FastSparseVector<T>& v) {
  print(out, v);
  return out;
}

#endif
