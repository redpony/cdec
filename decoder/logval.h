#ifndef LOGVAL_H_
#define LOGVAL_H_

#include <cmath>
#include <limits>

template <typename T>
class LogVal {
 public:
  LogVal() : v_(-std::numeric_limits<T>::infinity()) {}
  explicit LogVal(double x) : v_(std::log(x)) {}
  LogVal<T>(const LogVal<T>& o) : v_(o.v_) {}
  static LogVal<T> One() { return LogVal(1); }
  static LogVal<T> Zero() { return LogVal(); }

  void logeq(const T& v) { v_ = v; }

  LogVal& operator+=(const LogVal& a) {
    if (a.v_ == -std::numeric_limits<T>::infinity()) return *this;
    if (a.v_ < v_) {
      v_ = v_ + log1p(std::exp(a.v_ - v_));
    } else {
      v_ = a.v_ + log1p(std::exp(v_ - a.v_));
    }
    return *this;
  }

  LogVal& operator*=(const LogVal& a) {
    v_ += a.v_;
    return *this;
  }

  LogVal& operator*=(const T& a) {
    v_ += log(a);
    return *this;
  }

  LogVal& operator/=(const LogVal& a) {
    v_ -= a.v_;
    return *this;
  }

  LogVal& poweq(const T& power) {
    if (power == 0) v_ = 0; else v_ *= power;
    return *this;
  }

  LogVal pow(const T& power) const {
    LogVal res = *this;
    res.poweq(power);
    return res;
  }

  operator T() const {
    return std::exp(v_);
  }

  T v_;
};

template<typename T>
LogVal<T> operator+(const LogVal<T>& o1, const LogVal<T>& o2) {
  LogVal<T> res(o1);
  res += o2;
  return res;
}

template<typename T>
LogVal<T> operator*(const LogVal<T>& o1, const LogVal<T>& o2) {
  LogVal<T> res(o1);
  res *= o2;
  return res;
}

template<typename T>
LogVal<T> operator*(const LogVal<T>& o1, const T& o2) {
  LogVal<T> res(o1);
  res *= o2;
  return res;
}

template<typename T>
LogVal<T> operator*(const T& o1, const LogVal<T>& o2) {
  LogVal<T> res(o2);
  res *= o1;
  return res;
}

template<typename T>
LogVal<T> operator/(const LogVal<T>& o1, const LogVal<T>& o2) {
  LogVal<T> res(o1);
  res /= o2;
  return res;
}

template<typename T>
T log(const LogVal<T>& o) {
  return o.v_;
}

template <typename T>
LogVal<T> pow(const LogVal<T>& b, const T& e) {
  return b.pow(e);
}

template <typename T>
bool operator<(const LogVal<T>& lhs, const LogVal<T>& rhs) {
  return (lhs.v_ < rhs.v_);
}

template <typename T>
bool operator<=(const LogVal<T>& lhs, const LogVal<T>& rhs) {
  return (lhs.v_ <= rhs.v_);
}

template <typename T>
bool operator>(const LogVal<T>& lhs, const LogVal<T>& rhs) {
  return (lhs.v_ > rhs.v_);
}

template <typename T>
bool operator>=(const LogVal<T>& lhs, const LogVal<T>& rhs) {
  return (lhs.v_ >= rhs.v_);
}

template <typename T>
bool operator==(const LogVal<T>& lhs, const LogVal<T>& rhs) {
  return (lhs.v_ == rhs.v_);
}

template <typename T>
bool operator!=(const LogVal<T>& lhs, const LogVal<T>& rhs) {
  return (lhs.v_ != rhs.v_);
}

#endif
