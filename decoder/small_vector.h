#ifndef _SMALL_VECTOR_H_

#include <streambuf>  // std::max - where to get this?
#include <cstring>
#include <cassert>
#include <limits.h>

//sizeof(T)/sizeof(T*)>1?sizeof(T)/sizeof(T*):1
template <class T,int SV_MAX=2 >
class SmallVector {
  typedef unsigned short uint16_t;
 public:
  typedef SmallVector<T,SV_MAX> Self;
  SmallVector() : size_(0) {}

  explicit SmallVector(size_t s, int v = 0) : size_(s) {
    assert(s < 0x80);
    if (s <= SV_MAX) {
      for (int i = 0; i < s; ++i) data_.vals[i] = v;
    } else {
      capacity_ = s;
      size_ = s;
      data_.ptr = new int[s];
      for (int i = 0; i < size_; ++i) data_.ptr[i] = v;
    }
  }

  SmallVector(const Self& o) : size_(o.size_) {
    if (size_ <= SV_MAX) {
      for (int i = 0; i < SV_MAX; ++i) data_.vals[i] = o.data_.vals[i];
    } else {
      capacity_ = size_ = o.size_;
      data_.ptr = new int[capacity_];
      std::memcpy(data_.ptr, o.data_.ptr, size_ * sizeof(int));
    }
  }

  const Self& operator=(const Self& o) {
    if (size_ <= SV_MAX) {
      if (o.size_ <= SV_MAX) {
        size_ = o.size_;
        for (int i = 0; i < SV_MAX; ++i) data_.vals[i] = o.data_.vals[i];
      } else {
        capacity_ = size_ = o.size_;
        data_.ptr = new int[capacity_];
        std::memcpy(data_.ptr, o.data_.ptr, size_ * sizeof(int));
      }
    } else {
      if (o.size_ <= SV_MAX) {
        delete[] data_.ptr;
        size_ = o.size_;
        for (int i = 0; i < size_; ++i) data_.vals[i] = o.data_.vals[i];
      } else {
        if (capacity_ < o.size_) {
          delete[] data_.ptr;
          capacity_ = o.size_;
          data_.ptr = new int[capacity_];
        }
        size_ = o.size_;
        for (int i = 0; i < size_; ++i)
          data_.ptr[i] = o.data_.ptr[i];
      }
    }
    return *this;
  }

  ~SmallVector() {
    if (size_ <= SV_MAX) return;
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
    int* tmp = new int[new_cap];
    std::memcpy(tmp, data_.ptr, capacity_ * sizeof(int));
    delete[] data_.ptr;
    data_.ptr = tmp;
    capacity_ = new_cap;
  }

  inline void copy_vals_to_ptr() {
    capacity_ = SV_MAX * 2;
    int* tmp = new int[capacity_];
    for (int i = 0; i < SV_MAX; ++i) tmp[i] = data_.vals[i];
    data_.ptr = tmp;
  }

  inline void push_back(int v) {
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

  int& back() { return this->operator[](size_ - 1); }
  const int& back() const { return this->operator[](size_ - 1); }
  int& front() { return this->operator[](0); }
  const int& front() const { return this->operator[](0); }

  void resize(size_t s, int v = 0) {
    if (s <= SV_MAX) {
      if (size_ > SV_MAX) {
        int tmp[SV_MAX];
        for (int i = 0; i < s; ++i) tmp[i] = data_.ptr[i];
        delete[] data_.ptr;
        for (int i = 0; i < s; ++i) data_.vals[i] = tmp[i];
        size_ = s;
        return;
      }
      if (s <= size_) {
        size_ = s;
        return;
      } else {
        for (int i = size_; i < s; ++i)
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
        for (int i = size_; i < s; ++i)
          data_.ptr[i] = v;
      }
      size_ = s;
    }
  }

  int& operator[](size_t i) {
    if (size_ <= SV_MAX) return data_.vals[i];
    return data_.ptr[i];
  }

  const int& operator[](size_t i) const {
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

 private:
  union StorageType {
    int vals[SV_MAX];
    int* ptr;
  };
  StorageType data_;
  uint16_t size_;
  uint16_t capacity_;  // only defined when size_ >= __SV_MAX_STATIC
};

typedef SmallVector<int,2> SmallVectorInt;

#endif
