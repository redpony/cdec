#ifndef _SPARSE_VECTOR_H_
#define _SPARSE_VECTOR_H_

// this is a modified version of code originally written
// by Phil Blunsom

#include <iostream>
#include <map>
#include <vector>
#include <valarray>

#include "fdict.h"

template <typename T>
class SparseVector {
public:
    SparseVector() {}

    const T operator[](int index) const {
      typename std::map<int, T>::const_iterator found = _values.find(index);
      if (found == _values.end())
        return T(0);
      else
        return found->second;
    }

    void set_value(int index, const T &value) {
        _values[index] = value;
    }

    void add_value(int index, const T &value) {
        _values[index] += value;
    }

    T value(int index) const {
        typename std::map<int, T>::const_iterator found = _values.find(index);
        if (found != _values.end())
            return found->second;
        else
            return T(0);
    }

    void store(std::valarray<T>* target) const {
      (*target) *= 0;
      for (typename std::map<int, T>::const_iterator 
              it = _values.begin(); it != _values.end(); ++it) {
        if (it->first >= target->size()) break;
        (*target)[it->first] = it->second;
      }
    }

    int max_index() const {
        if (_values.empty()) return 0;
        typename std::map<int, T>::const_iterator found =_values.end();
        --found;
        return found->first;
    }

    // dot product with a unit vector of the same length
    // as the sparse vector
    T dot() const {
        T sum = 0;
        for (typename std::map<int, T>::const_iterator 
                it = _values.begin(); it != _values.end(); ++it)
            sum += it->second;
        return sum;
    }

    template<typename S>
    S dot(const SparseVector<S> &vec) const {
        S sum = 0;
        for (typename std::map<int, T>::const_iterator 
                it = _values.begin(); it != _values.end(); ++it)
        {
            typename std::map<int, T>::const_iterator 
                found = vec._values.find(it->first);
            if (found != vec._values.end())
                sum += it->second * found->second;
        }
        return sum;
    }
    
    template<typename S>
    S dot(const std::vector<S> &vec) const {
        S sum = 0;
        for (typename std::map<int, T>::const_iterator 
                it = _values.begin(); it != _values.end(); ++it)
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
        for (typename std::map<int, T>::const_iterator 
                it = _values.begin(); it != _values.end(); ++it)
            sum += it->second * vec[it->first];
        std::cout << "dot(*vec) " << sum << std::endl;
        return sum;
    }

    T l1norm() const {
      T sum = 0;
      for (typename std::map<int, T>::const_iterator 
              it = _values.begin(); it != _values.end(); ++it)
        sum += fabs(it->second);
      return sum;
    }
    
    T l2norm() const {
      T sum = 0;
      for (typename std::map<int, T>::const_iterator 
              it = _values.begin(); it != _values.end(); ++it)
        sum += it->second * it->second;
      return sqrt(sum);
    }
    
    SparseVector<T> &operator+=(const SparseVector<T> &other) {
        for (typename std::map<int, T>::const_iterator 
                it = other._values.begin(); it != other._values.end(); ++it)
        {
            T v = (_values[it->first] += it->second);
            if (v == 0)
                _values.erase(it->first);
        }
        return *this;
    }

    SparseVector<T> &operator-=(const SparseVector<T> &other) {
        for (typename std::map<int, T>::const_iterator 
                it = other._values.begin(); it != other._values.end(); ++it)
        {
            T v = (_values[it->first] -= it->second);
            if (v == 0)
                _values.erase(it->first);
        }
        return *this;
    }

    SparseVector<T> &operator-=(const double &x) {
        for (typename std::map<int, T>::iterator 
                it = _values.begin(); it != _values.end(); ++it)
            it->second -= x;
        return *this;
    }

    SparseVector<T> &operator+=(const double &x) {
        for (typename std::map<int, T>::iterator 
                it = _values.begin(); it != _values.end(); ++it)
            it->second += x;
        return *this;
    }

    SparseVector<T> &operator/=(const double &x) {
        for (typename std::map<int, T>::iterator 
                it = _values.begin(); it != _values.end(); ++it)
            it->second /= x;
        return *this;
    }

    SparseVector<T> &operator*=(const T& x) {
        for (typename std::map<int, T>::iterator 
                it = _values.begin(); it != _values.end(); ++it)
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
        for (typename std::map<int, T>::const_iterator 
                it = _values.begin(); it != _values.end(); ++it)
            out << (it == _values.begin() ? "" : ";")
	        << FD::Convert(it->first) << '=' << it->second;
        return out;
    }

    bool operator<(const SparseVector<T> &other) const {
        typename std::map<int, T>::const_iterator it = _values.begin();
        typename std::map<int, T>::const_iterator other_it = other._values.begin();

        for (; it != _values.end() && other_it != other._values.end(); ++it, ++other_it)
        {
            if (it->first < other_it->first) return true;
            if (it->first > other_it->first) return false;
            if (it->second < other_it->second) return true;
            if (it->second > other_it->second) return false;
        }
        return _values.size() < other._values.size();
    }

    int num_active() const { return _values.size(); }
    bool empty() const { return _values.empty(); }

    typedef typename std::map<int, T>::const_iterator const_iterator;
    const_iterator begin() const { return _values.begin(); }
    const_iterator end() const { return _values.end(); }

    void clear() {
        _values.clear();
    }

    void swap(SparseVector<T>& other) {
      _values.swap(other._values);
    }

private:
    std::map<int, T> _values;
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
