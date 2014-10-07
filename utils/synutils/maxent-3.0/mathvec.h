#ifndef _MATH_VECTOR_H_
#define _MATH_VECTOR_H_

#include <vector>
#include <iostream>
#include <cassert>

class Vec
{
private:
  std::vector<double> _v;
public:
  Vec(const size_t n = 0, const double val = 0) { _v.resize(n, val); }
  Vec(const std::vector<double> & v) : _v(v)    {}
  const std::vector<double> & STLVec() const { return _v; }
  std::vector<double>       & STLVec()       { return _v; }
  size_t Size() const { return _v.size(); }
  double       & operator[](int i)       { return _v[i]; }
  const double & operator[](int i) const { return _v[i]; }
  Vec & operator+=(const Vec & b) {
    assert(b.Size() == _v.size());
    for (size_t i = 0; i < _v.size(); i++) {
      _v[i] += b[i];
    }
    return *this;
  }
  Vec & operator*=(const double c) {
    for (size_t i = 0; i < _v.size(); i++) {
      _v[i] *= c;
    }
    return *this;
  }
  void Project(const Vec & y) {
    for (size_t i = 0; i < _v.size(); i++) {
      //      if (sign(_v[i]) != sign(y[i])) _v[i] = 0;
      if (_v[i] * y[i] <=0) _v[i] = 0;
    }
  }
};

inline double dot_product(const Vec & a, const Vec & b)
{
  double sum = 0;
  for (size_t i = 0; i < a.Size(); i++) {
    sum += a[i] * b[i];
  }
  return sum;
}

inline std::ostream & operator<<(std::ostream & s, const Vec & a)
{
  s << "(";
  for (size_t i = 0; i < a.Size(); i++) {
    if (i != 0) s << ", ";
    s << a[i];
  }
  s << ")";
  return s;
}

inline const Vec operator+(const Vec & a, const Vec & b)
{
  Vec v(a.Size());
  assert(a.Size() == b.Size());
  for (size_t i = 0; i < a.Size(); i++) {
    v[i] = a[i] + b[i];
  }
  return v;
}

inline const Vec operator-(const Vec & a, const Vec & b)
{
  Vec v(a.Size());
  assert(a.Size() == b.Size());
  for (size_t i = 0; i < a.Size(); i++) {
    v[i] = a[i] - b[i];
  }
  return v;
}

inline const Vec operator*(const Vec & a, const double c)
{
  Vec v(a.Size());
  for (size_t i = 0; i < a.Size(); i++) {
    v[i] = a[i] * c;
  }
  return v;
}

inline const Vec operator*(const double c, const Vec & a)
{
  return a * c;
}



#endif
