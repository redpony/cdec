#ifndef _SMALL_VECTOR_H_

#include <streambuf>  // std::max - where to get this?
#include <cstring>
#include <cassert>
#include <limits.h>

#define __SV_MAX_STATIC 2

class SmallVector {

 public:
  SmallVector() : size_(0) {}

  explicit SmallVector(size_t s, int v = 0) : size_(s) {
    assert(s < 0x80);
    if (s <= __SV_MAX_STATIC) {
      for (int i = 0; i < s; ++i) data_.vals[i] = v;
    } else {
      capacity_ = s;
      size_ = s;
      data_.ptr = new int[s];
      for (int i = 0; i < size_; ++i) data_.ptr[i] = v;
    }
  }

  SmallVector(const SmallVector& o) : size_(o.size_) {
    if (size_ <= __SV_MAX_STATIC) {
      for (int i = 0; i < __SV_MAX_STATIC; ++i) data_.vals[i] = o.data_.vals[i];
    } else {
      capacity_ = size_ = o.size_;
      data_.ptr = new int[capacity_];
      std::memcpy(data_.ptr, o.data_.ptr, size_ * sizeof(int));
    }
  }

  const SmallVector& operator=(const SmallVector& o) {
    if (size_ <= __SV_MAX_STATIC) {
      if (o.size_ <= __SV_MAX_STATIC) {
        size_ = o.size_;
        for (int i = 0; i < __SV_MAX_STATIC; ++i) data_.vals[i] = o.data_.vals[i];
      } else {
        capacity_ = size_ = o.size_;
        data_.ptr = new int[capacity_];
        std::memcpy(data_.ptr, o.data_.ptr, size_ * sizeof(int));
      }
    } else {
      if (o.size_ <= __SV_MAX_STATIC) {
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
    if (size_ <= __SV_MAX_STATIC) return;
    delete[] data_.ptr;
  }

  void clear() {
    if (size_ > __SV_MAX_STATIC) {
      delete[] data_.ptr;
    }
    size_ = 0;
  }

  bool empty() const { return size_ == 0; }
  size_t size() const { return size_; }

  inline void ensure_capacity(uint16_t min_size) {
    assert(min_size > __SV_MAX_STATIC);
    if (min_size < capacity_) return;
    uint16_t new_cap = std::max(static_cast<uint16_t>(capacity_ << 1), min_size);
    int* tmp = new int[new_cap];
    std::memcpy(tmp, data_.ptr, capacity_ * sizeof(int));
    delete[] data_.ptr;
    data_.ptr = tmp;
    capacity_ = new_cap;
  }

  inline void copy_vals_to_ptr() {
    capacity_ = __SV_MAX_STATIC * 2;
    int* tmp = new int[capacity_];
    for (int i = 0; i < __SV_MAX_STATIC; ++i) tmp[i] = data_.vals[i];
    data_.ptr = tmp;
  }

  inline void push_back(int v) {
    if (size_ < __SV_MAX_STATIC) {
      data_.vals[size_] = v;
      ++size_;
      return;
    } else if (size_ == __SV_MAX_STATIC) {
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
    if (s <= __SV_MAX_STATIC) {
      if (size_ > __SV_MAX_STATIC) {
        int tmp[__SV_MAX_STATIC];
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
      if (size_ <= __SV_MAX_STATIC)
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
    if (size_ <= __SV_MAX_STATIC) return data_.vals[i];
    return data_.ptr[i];
  }

  const int& operator[](size_t i) const {
    if (size_ <= __SV_MAX_STATIC) return data_.vals[i];
    return data_.ptr[i];
  }

  bool operator==(const SmallVector& o) const {
    if (size_ != o.size_) return false;
    if (size_ <= __SV_MAX_STATIC) {
      for (size_t i = 0; i < size_; ++i)
        if (data_.vals[i] != o.data_.vals[i]) return false;
      return true;
    } else {
      for (size_t i = 0; i < size_; ++i)
        if (data_.ptr[i] != o.data_.ptr[i]) return false;
      return true;
    }
  }

 private:
  uint16_t capacity_;  // only defined when size_ >= __SV_MAX_STATIC
  uint16_t size_;
  union StorageType {
    int vals[__SV_MAX_STATIC];
    int* ptr;
  };
  StorageType data_;

};

inline bool operator!=(const SmallVector& a, const SmallVector& b) {
  return !(a==b);
}

#endif
