#ifndef _SPARSE_VECTOR_H_
#define _SPARSE_VECTOR_H_

// this is a modified version of code originally written
// by Phil Blunsom

#include <iostream>
#include <map>
#include <tr1/unordered_map>
#include <vector>
#include <valarray>

#include "fdict.h"

template <typename T>
class SparseVector {
public:
  typedef std::map<int, T> MapType;
  typedef typename std::map<int, T>::const_iterator const_iterator;
  SparseVector() {}

  const T operator[](int index) const {
    typename MapType::const_iterator found = values_.find(index);
    if (found == values_.end())
      return T(0);
    else
      return found->second;
  }

  void set_value(int index, const T &value) {
    values_[index] = value;
  }

    T add_value(int index, const T &value) {
        return values_[index] += value;
    }

    T value(int index) const {
        typename MapType::const_iterator found = values_.find(index);
        if (found != values_.end())
            return found->second;
        else
            return T(0);
    }

    void store(std::valarray<T>* target) const {
      (*target) *= 0;
      for (typename MapType::const_iterator 
              it = values_.begin(); it != values_.end(); ++it) {
        if (it->first >= target->size()) break;
        (*target)[it->first] = it->second;
      }
    }

    int max_index() const {
        if (values_.empty()) return 0;
        typename MapType::const_iterator found =values_.end();
        --found;
        return found->first;
    }

    // dot product with a unit vector of the same length
    // as the sparse vector
    T dot() const {
        T sum = 0;
        for (typename MapType::const_iterator 
                it = values_.begin(); it != values_.end(); ++it)
            sum += it->second;
        return sum;
    }

    template<typename S>
    S dot(const SparseVector<S> &vec) const {
        S sum = 0;
        for (typename MapType::const_iterator 
                it = values_.begin(); it != values_.end(); ++it)
        {
            typename MapType::const_iterator 
                found = vec.values_.find(it->first);
            if (found != vec.values_.end())
                sum += it->second * found->second;
        }
        return sum;
    }
    
    template<typename S>
    S dot(const std::vector<S> &vec) const {
        S sum = 0;
        for (typename MapType::const_iterator 
                it = values_.begin(); it != values_.end(); ++it)
        {
            if (it->first < static_cast<int>(vec.size()))
                sum += it->second * vec[it->first];
        }
        return sum;
    }

    template<typename S>
    S dot(const S *vec) const {
        // this is not range checked!
        S sum = 0;
        for (typename MapType::const_iterator 
                it = values_.begin(); it != values_.end(); ++it)
            sum += it->second * vec[it->first];
        std::cout << "dot(*vec) " << sum << std::endl;
        return sum;
    }

    T l1norm() const {
      T sum = 0;
      for (typename MapType::const_iterator 
              it = values_.begin(); it != values_.end(); ++it)
        sum += fabs(it->second);
      return sum;
    }
    
    T l2norm() const {
      T sum = 0;
      for (typename MapType::const_iterator 
              it = values_.begin(); it != values_.end(); ++it)
        sum += it->second * it->second;
      return sqrt(sum);
    }
    
    SparseVector<T> &operator+=(const SparseVector<T> &other) {
        for (typename MapType::const_iterator 
                it = other.values_.begin(); it != other.values_.end(); ++it)
        {
            T v = (values_[it->first] += it->second);
            if (v == T())
                values_.erase(it->first);
        }
        return *this;
    }

    SparseVector<T> &operator-=(const SparseVector<T> &other) {
        for (typename MapType::const_iterator 
                it = other.values_.begin(); it != other.values_.end(); ++it)
        {
            T v = (values_[it->first] -= it->second);
            if (v == T(0))
                values_.erase(it->first);
        }
        return *this;
    }

    SparseVector<T> &operator-=(const double &x) {
        for (typename MapType::iterator 
                it = values_.begin(); it != values_.end(); ++it)
            it->second -= x;
        return *this;
    }

    SparseVector<T> &operator+=(const double &x) {
        for (typename MapType::iterator 
                it = values_.begin(); it != values_.end(); ++it)
            it->second += x;
        return *this;
    }

    SparseVector<T> &operator/=(const T &x) {
        for (typename MapType::iterator 
                it = values_.begin(); it != values_.end(); ++it)
            it->second /= x;
        return *this;
    }

    SparseVector<T> &operator*=(const T& x) {
        for (typename MapType::iterator 
                it = values_.begin(); it != values_.end(); ++it)
            it->second *= x;
        return *this;
    }

    SparseVector<T> operator+(const double &x) const {
        SparseVector<T> result = *this;
        return result += x;
    }

    SparseVector<T> operator-(const double &x) const {
        SparseVector<T> result = *this;
        return result -= x;
    }

    SparseVector<T> operator/(const double &x) const {
        SparseVector<T> result = *this;
        return result /= x;
    }

    std::ostream &operator<<(std::ostream &out) const {
        bool first = true;
        for (typename MapType::const_iterator 
                it = values_.begin(); it != values_.end(); ++it) {
          // by definition feature id 0 is a dummy value
          if (it->first == 0) continue;
          out << (first ? "" : ";")
	      << FD::Convert(it->first) << '=' << it->second;
          first = false;
        }
        return out;
    }

    bool operator<(const SparseVector<T> &other) const {
        typename MapType::const_iterator it = values_.begin();
        typename MapType::const_iterator other_it = other.values_.begin();

        for (; it != values_.end() && other_it != other.values_.end(); ++it, ++other_it)
        {
            if (it->first < other_it->first) return true;
            if (it->first > other_it->first) return false;
            if (it->second < other_it->second) return true;
            if (it->second > other_it->second) return false;
        }
        return values_.size() < other.values_.size();
    }

    int num_active() const { return values_.size(); }
    bool empty() const { return values_.empty(); }

    const_iterator begin() const { return values_.begin(); }
    const_iterator end() const { return values_.end(); }

    void clear() {
        values_.clear();
    }
    void clear_value(int index) {
      values_.erase(index);
    }

    void swap(SparseVector<T>& other) {
      values_.swap(other.values_);
    }

private:
  MapType values_;
};

template <typename T>
SparseVector<T> operator+(const SparseVector<T>& a, const SparseVector<T>& b) {
  SparseVector<T> result = a;
  return result += b;
}

template <typename T>
SparseVector<T> operator*(const SparseVector<T>& a, const double& b) {
  SparseVector<T> result = a;
  return result *= b;
}

template <typename T>
SparseVector<T> operator*(const SparseVector<T>& a, const T& b) {
  SparseVector<T> result = a;
  return result *= b;
}

template <typename T>
SparseVector<T> operator*(const double& a, const SparseVector<T>& b) {
  SparseVector<T> result = b;
  return result *= a;
}

template <typename T>
std::ostream &operator<<(std::ostream &out, const SparseVector<T> &vec)
{
    return vec.operator<<(out);
}

namespace B64 {
  void Encode(double objective, const SparseVector<double>& v, std::ostream* out);
  // returns false if failed to decode
  bool Decode(double* objective, SparseVector<double>* v, const char* data, size_t size);
}

#endif
