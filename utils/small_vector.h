#ifndef _SMALL_VECTOR_H_
#define _SMALL_VECTOR_H_

/* REQUIRES that T is POD (can be memcpy).  won't work (yet) due to union with SMALL_VECTOR_POD==0 - may be possible to handle movable types that have ctor/dtor, by using  explicit allocation, ctor/dtor calls.  but for now JUST USE THIS FOR no-meaningful ctor/dtor POD types.

   stores small element (<=SV_MAX items) vectors inline.  recommend SV_MAX=sizeof(T)/sizeof(T*)>1?sizeof(T)/sizeof(T*):1.  may not work if SV_MAX==0.
 */

#define SMALL_VECTOR_POD 1

#include <streambuf>  // std::max - where to get this?
#include <cstring>
#include <cassert>
#include <stdint.h>
#include <new>
#include <stdint.h>
#include <boost/functional/hash.hpp>

//sizeof(T)/sizeof(T*)>1?sizeof(T)/sizeof(T*):1

template <class T,int SV_MAX=2>
class SmallVector {
//  typedef unsigned short uint16_t;
  void Alloc(size_t s) {
    size_=s;
    assert(s < 0xA000);
    if (s>SV_MAX) {
      capacity_ = s;
      size_ = s;
      data_.ptr = new T[s]; // TODO: replace this with allocator or ::operator new(sizeof(T)*s) everywhere
    }
  }

 public:
  typedef SmallVector<T,SV_MAX> Self;
  SmallVector() : size_(0) {}

  typedef T const* const_iterator;
  typedef T* iterator;
  typedef T value_type;
  typedef T &reference;
  typedef T const& const_reference;

  T *begin() { return size_>SV_MAX?data_.ptr:data_.vals; }
  T const* begin() const { return const_cast<Self*>(this)->begin(); }
  T *end() { return begin()+size_; }
  T const* end() const { return begin()+size_; }

  explicit SmallVector(size_t s) {
    Alloc(s);
    if (s <= SV_MAX) {
      for (unsigned i = 0; i < s; ++i) new(&data_.vals[i]) T();
    } //TODO: if alloc were raw space, construct here.
  }

  SmallVector(size_t s, T const& v) {
    Alloc(s);
    if (s <= SV_MAX) {
      for (unsigned i = 0; i < s; ++i) data_.vals[i] = v;
    } else {
      for (unsigned i = 0; i < size_; ++i) data_.ptr[i] = v;
    }
  }

  //TODO: figure out iterator traits to allow this to be selcted for any iterator range
  template <class I>
  SmallVector(I const* begin,I const* end) {
    unsigned s=end-begin;
    Alloc(s);
    if (s <= SV_MAX) {
      for (unsigned i = 0; i < s; ++i,++begin) data_.vals[i] = *begin;
    } else
      for (unsigned i = 0; i < s; ++i,++begin) data_.ptr[i] = *begin;
  }

  SmallVector(const Self& o) : size_(o.size_) {
    if (size_ <= SV_MAX) {
      std::memcpy(data_.vals,o.data_.vals,size_*sizeof(T));
//      for (int i = 0; i < size_; ++i) data_.vals[i] = o.data_.vals[i];
    } else {
      capacity_ = size_ = o.size_;
      data_.ptr = new T[capacity_];
      std::memcpy(data_.ptr, o.data_.ptr, size_ * sizeof(T));
    }
  }

  //TODO: test.  this invalidates more iterators than std::vector since resize may move from ptr to vals.
  T *erase(T *b) {
    return erase(b,b+1);
  }
  T *erase(T *b,T* e) {
    T *tb=begin(),*te=end();
    int nbefore=b-tb;
    if (e==te) {
      resize(nbefore);
    } else {
      int nafter=te-e;
      std::memmove(b,e,nafter*sizeof(T));
      resize(nbefore+nafter);
    }
    return begin()+nbefore;
  }

  const Self& operator=(const Self& o) {
    if (size_ <= SV_MAX) {
      if (o.size_ <= SV_MAX) {
        size_ = o.size_;
        for (unsigned i = 0; i < SV_MAX; ++i) data_.vals[i] = o.data_.vals[i];
      } else {
        capacity_ = size_ = o.size_;
        data_.ptr = new T[capacity_];
        std::memcpy(data_.ptr, o.data_.ptr, size_ * sizeof(T));
      }
    } else {
      if (o.size_ <= SV_MAX) {
        delete[] data_.ptr;
        size_ = o.size_;
        for (unsigned i = 0; i < size_; ++i) data_.vals[i] = o.data_.vals[i];
      } else {
        if (capacity_ < o.size_) {
          delete[] data_.ptr;
          capacity_ = o.size_;
          data_.ptr = new T[capacity_];
        }
        size_ = o.size_;
        for (unsigned i = 0; i < size_; ++i)
          data_.ptr[i] = o.data_.ptr[i];
      }
    }
    return *this;
  }

  ~SmallVector() {
    if (size_ <= SV_MAX) {
      // skip if pod?  yes, we required pod anyway.  no need to destruct
#if !SMALL_VECTOR_POD
      for (unsigned i=0;i<size_;++i) data_.vals[i].~T();
#endif
    } else
      delete[] data_.ptr;
  }

  void clear() {
    if (size_ > SV_MAX) {
      delete[] data_.ptr;
    }
    size_ = 0;
  }

  bool empty() const { return size_ == 0; }
  size_t size() const { return size_; }

  inline void ensure_capacity(uint16_t min_size) {
    assert(min_size > SV_MAX);
    if (min_size < capacity_) return;
    uint16_t new_cap = std::max(static_cast<uint16_t>(capacity_ << 1), min_size);
    T* tmp = new T[new_cap];
    std::memcpy(tmp, data_.ptr, capacity_ * sizeof(T));
    delete[] data_.ptr;
    data_.ptr = tmp;
    capacity_ = new_cap;
  }

private:
  inline void copy_vals_to_ptr() {
    capacity_ = SV_MAX * 2;
    T* tmp = new T[capacity_];
    for (unsigned i = 0; i < SV_MAX; ++i) tmp[i] = data_.vals[i];
    data_.ptr = tmp;
  }
  inline void ptr_to_small() {
    assert(size_<=SV_MAX);
    int *tmp=data_.ptr;
    for (unsigned i=0;i<size_;++i)
      data_.vals[i]=tmp[i];
    delete[] tmp;
  }

public:

  inline void push_back(T const& v) {
    if (size_ < SV_MAX) {
      data_.vals[size_] = v;
      ++size_;
      return;
    } else if (size_ == SV_MAX) {
      copy_vals_to_ptr();
    } else if (size_ == capacity_) {
      ensure_capacity(size_ + 1);
    }
    data_.ptr[size_] = v;
    ++size_;
  }

  T& back() { return this->operator[](size_ - 1); }
  const T& back() const { return this->operator[](size_ - 1); }
  T& front() { return this->operator[](0); }
  const T& front() const { return this->operator[](0); }

  void pop_back() {
    assert(size_>0);
    --size_;
    if (size_==SV_MAX)
      ptr_to_small();
  }

  void compact() {
    compact(size_);
  }

  // size must be <= size_ - TODO: test
  void compact(uint16_t size) {
    assert(size<=size_);
    if (size_>SV_MAX) {
      size_=size;
      if (size<=SV_MAX)
        ptr_to_small();
    } else
      size_=size;
  }

  void resize(size_t s, int v = 0) {
    if (s <= SV_MAX) {
      if (size_ > SV_MAX) {
        T *tmp=data_.ptr;
        for (unsigned i = 0; i < s; ++i) data_.vals[i] = tmp[i];
        delete[] tmp;
        size_ = s;
        return;
      }
      if (s <= size_) {
        size_ = s;
        return;
      } else {
        for (unsigned i = size_; i < s; ++i)
          data_.vals[i] = v;
        size_ = s;
        return;
      }
    } else {
      if (size_ <= SV_MAX)
        copy_vals_to_ptr();
      if (s > capacity_)
        ensure_capacity(s);
      if (s > size_) {
        for (unsigned i = size_; i < s; ++i)
          data_.ptr[i] = v;
      }
      size_ = s;
    }
  }

  T& operator[](size_t i) {
    if (size_ <= SV_MAX) return data_.vals[i];
    return data_.ptr[i];
  }

  const T& operator[](size_t i) const {
    if (size_ <= SV_MAX) return data_.vals[i];
    return data_.ptr[i];
  }

  bool operator==(const Self& o) const {
    if (size_ != o.size_) return false;
    if (size_ <= SV_MAX) {
      for (size_t i = 0; i < size_; ++i)
        if (data_.vals[i] != o.data_.vals[i]) return false;
      return true;
    } else {
      for (size_t i = 0; i < size_; ++i)
        if (data_.ptr[i] != o.data_.ptr[i]) return false;
      return true;
    }
  }

  friend bool operator!=(const Self& a, const Self& b) {
    return !(a==b);
  }

  inline void swap(Self& o) {
    const unsigned s=sizeof(SmallVector<T,SV_MAX>);
    char tmp[s];
    void *pt=static_cast<void*>(tmp);
    void *pa=static_cast<void*>(this);
    void *pb=static_cast<void*>(&o);
    std::memcpy(pt,pa,s);
    std::memcpy(pa,pb,s);
    std::memcpy(pb,pt,s);
  }

  inline std::size_t hash_impl() const {
    using namespace boost;
    if (size_==0) return 0;
    if (size_==1) return hash_value(data_.vals[0]);
    if (size_ <= SV_MAX)
      return hash_range(data_.vals,data_.vals+size_);
    return hash_range(data_.ptr,data_.ptr+size_);
  }

 private:
  union StorageType {
    T vals[SV_MAX];
    T* ptr;
  };
  StorageType data_;
  uint16_t size_;
  uint16_t capacity_;  // only defined when size_ > __SV_MAX_STATIC
};

namespace boost {
// shouldn't need to nest this, but getting into trouble with tr1::hash linkage
}

template <class T,int M>
inline std::size_t hash_value(SmallVector<T,M> const& x) {
  return x.hash_impl();
}

template <class T,int M>
inline void swap(SmallVector<T,M> &a,SmallVector<T,M> &b) {
  a.swap(b);
}

typedef SmallVector<int,2> SmallVectorInt;
typedef SmallVector<unsigned,2> SmallVectorUnsigned;

template <class T,int M>
void memcpy(void *out,SmallVector<T,M> const& v) {
  std::memcpy(out,v.begin(),v.size()*sizeof(T));
}

#endif
