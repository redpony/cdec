#ifndef _FAST_SPARSE_VECTOR_H_
#define _FAST_SPARSE_VECTOR_H_

#include <map>
#include <cassert>

#include <boost/static_assert.hpp>

// these are architecture dependent, they should be
// detected in some way but it's probably easiest (for me)
// to just set them
#define L2_CACHE_LINE 128

// this should just be a typedef to pair<int,float> on the new c++
template <typename T>
struct PairIntFloat {
  int first;
  T second;
  const PairIntFloat& operator=(const std::pair<const int, float>& v) {
    first = v.first;
    second = v.second;
    return *this;
  }
  operator const std::pair<const int, float>&() const {
    return *reinterpret_cast<const std::pair<const int, float>*>(this);
  }
};
BOOST_STATIC_ASSERT(sizeof(PairIntFloat<float>) == sizeof(std::pair<int,float>));

template <typename T, int LOCAL_MAX = (sizeof(T) == sizeof(float) ? 15 : 7)>
class FastSparseVector {
 public:
  FastSparseVector() : local_size_(0), is_local_(true) {}
  ~FastSparseVector() {
    if (!is_local_) delete data_.rbmap;
  }
  FastSparseVector(const FastSparseVector& other) {
    memcpy(this, &other, sizeof(FastSparseVector));
    if (is_local_) return;
    data_.rbmap = new std::map<int, T>(*data_.rbmap);
  }
  const FastSparseVector& operator=(const FastSparseVector& other) {
    if (!is_local_) delete data_.rbmap;
    memcpy(this, &other, sizeof(FastSparseVector));
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
        const PairIntFloat<T>& p = data_.local[i];
        if (p.first == k) return p.second;
      }
    } else {
      std::map<int, float>::const_iterator it = data_.rbmap->find(k);
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
    if (!is_local_) {
    } else if (!other.is_local_) {
    } else { // both local
    }
    return *this;
  }
 private:
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
      PairIntFloat<T>& p = data_.local[local_size_];
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
    PairIntFloat<T> local[LOCAL_MAX];
    std::map<int, T>* rbmap;
  } data_;
  unsigned char local_size_;
  bool is_local_;
};

namespace performance_checks {
  // if you get a failure on the next line, you should adjust LOCAL_MAX for
  // your architecture
  BOOST_STATIC_ASSERT(sizeof(FastSparseVector<float>) == L2_CACHE_LINE);
};

#endif
