#ifndef LOGVAL_H_
#define LOGVAL_H_

#define LOGVAL_CHECK_NEG false

#include <iostream>
#include <cstdlib>
#include <cmath>
#include <limits>

template <typename T>
class LogVal {
 public:
  LogVal() : s_(), v_(-std::numeric_limits<T>::infinity()) {}
  explicit LogVal(double x) : s_(std::signbit(x)), v_(s_ ? std::log(-x) : std::log(x)) {}
  LogVal(double lnx,bool sign) : s_(sign),v_(lnx) {}
  static LogVal<T> exp(T lnx) { return LogVal(lnx,false); }

  static LogVal<T> One() { return LogVal(1); }
  static LogVal<T> Zero() { return LogVal(); }
  static LogVal<T> e() { return LogVal(1,false); }
  void logeq(const T& v) { s_ = false; v_ = v; }

  LogVal& operator+=(const LogVal& a) {
    if (a.v_ == -std::numeric_limits<T>::infinity()) return *this;
    if (a.s_ == s_) {
      if (a.v_ < v_) {
        v_ = v_ + log1p(std::exp(a.v_ - v_));
      } else {
        v_ = a.v_ + log1p(std::exp(v_ - a.v_));
      }
    } else {
      if (a.v_ < v_) {
        v_ = v_ + log1p(-std::exp(a.v_ - v_));
      } else {
        v_ = a.v_ + log1p(-std::exp(v_ - a.v_));
        s_ = !s_;
      }
    }
    return *this;
  }

  LogVal& operator*=(const LogVal& a) {
    s_ = (s_ != a.s_);
    v_ += a.v_;
    return *this;
  }

  LogVal& operator/=(const LogVal& a) {
    s_ = (s_ != a.s_);
    v_ -= a.v_;
    return *this;
  }

  LogVal& operator-=(const LogVal& a) {
    LogVal b = a;
    b.invert();
    return *this += b;
  }

  LogVal& poweq(const T& power) {
#if LOGVAL_CHECK_NEG
    if (s_) {
      std::cerr << "poweq(T) not implemented when s_ is true\n";
      std::abort();
    } else
#endif
      v_ *= power;
    return *this;
  }

  void invert() { s_ = !s_; }

  LogVal pow(const T& power) const {
    LogVal res = *this;
    res.poweq(power);
    return res;
  }

  LogVal root(const T& root) const {
    return pow(1/root);
  }

  operator T() const {
    if (s_) return -std::exp(v_); else return std::exp(v_);
  }

  bool s_;
  T v_;
};

// copy elision - as opposed to explicit copy of LogVal<T> const& o1, we should be able to construct Logval r=a+(b+c) as a single result in place in r.  todo: return std::move(o1) - C++0x
template<typename T>
LogVal<T> operator+(LogVal<T> o1, const LogVal<T>& o2) {
  o1 += o2;
  return o1;
}

template<typename T>
LogVal<T> operator*(LogVal<T> o1, const LogVal<T>& o2) {
  o1 *= o2;
  return o1;
}

template<typename T>
LogVal<T> operator/(LogVal<T> o1, const LogVal<T>& o2) {
  o1 /= o2;
  return o1;
}

template<typename T>
LogVal<T> operator-(LogVal<T> o1, const LogVal<T>& o2) {
  o1 -= o2;
  return o1;
}

template<typename T>
T log(const LogVal<T>& o) {
#ifdef LOGVAL_CHECK_NEG
  if (o.s_) return log(-1.0);
#endif
  return o.v_;
}

template <typename T>
LogVal<T> pow(const LogVal<T>& b, const T& e) {
  return b.pow(e);
}

template <typename T>
bool operator<(const LogVal<T>& lhs, const LogVal<T>& rhs) {
  if (lhs.s_ == rhs.s_) {
    return (lhs.v_ < rhs.v_);
  } else {
    return lhs.s_ > rhs.s_;
  }
}

#if 0
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
#endif

template <typename T>
bool operator==(const LogVal<T>& lhs, const LogVal<T>& rhs) {
  return (lhs.v_ == rhs.v_) && (lhs.s_ == rhs.s_);
}

template <typename T>
bool operator!=(const LogVal<T>& lhs, const LogVal<T>& rhs) {
  return !(lhs == rhs);
}

#endif
