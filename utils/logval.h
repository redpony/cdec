#ifndef LOGVAL_H_
#define LOGVAL_H_

#define LOGVAL_CHECK_NEG false

#include <boost/functional/hash.hpp>
#include <iostream>
#include <cstdlib>
#include <cmath>
#include <limits>
#include <cassert>
#include "semiring.h"
#include "show.h"

//TODO: template for supporting negation or not - most uses are for nonnegative "probs" only; probably some 10-20% speedup available
template <class T>
class LogVal {
 public:
  void print(std::ostream &o) const {
    if (s_) o<<"(-)";
    o<<v_;
  }
  PRINT_SELF(LogVal<T>)

  typedef LogVal<T> Self;

  LogVal() : s_(), v_(LOGVAL_LOG0) {}
  LogVal(double x) : s_(std::signbit(x)), v_(s_ ? std::log(-x) : std::log(x)) {}
  const Self& operator=(double x) { s_ = std::signbit(x); v_ = s_ ? std::log(-x) : std::log(x); return *this; }
  LogVal(init_minus_1) : s_(true),v_(0) {  }
  LogVal(init_1) : s_(),v_(0) {  }
  LogVal(init_0) : s_(),v_(LOGVAL_LOG0) {  }
  LogVal(double lnx,bool sign) : s_(sign),v_(lnx) {}
  LogVal(double lnx,init_lnx) : s_(),v_(lnx) {}
  static Self exp(T lnx) { return Self(lnx,false); }

  // maybe the below are faster than == 1 and == 0.  i don't know.
  bool is_1() const { return v_==0&&s_==0; }
  bool is_0() const { return v_==LOGVAL_LOG0; }

  static Self One() { return Self(1); }
  static Self Zero() { return Self(); }
  static Self e() { return Self(1,false); }
  void logeq(const T& v) { s_ = false; v_ = v; }

  std::size_t hash_impl() const {
    using namespace boost;
    return hash_value(v_)+s_;
  }

  // just like std::signbit, negative means true.  weird, i know
  bool signbit() const {
    return s_;
  }
  friend inline bool signbit(Self const& x) { return x.signbit(); }

  Self& besteq(const Self& a) {
    assert(!a.s_ && !s_);
    if (a.v_ < v_)
      v_=a.v_;
    return *this;
  }

  Self& operator+=(const Self& a) {
    if (a.is_0()) return *this;
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

  Self& operator*=(const Self& a) {
    s_ = (s_ != a.s_);
    v_ += a.v_;
    return *this;
  }

  Self& operator/=(const Self& a) {
    s_ = (s_ != a.s_);
    v_ -= a.v_;
    return *this;
  }

  Self& operator-=(const Self& a) {
    Self b = a;
    b.negate();
    return *this += b;
  }

  // Self(fabs(log(x)),x.s_)
  friend Self abslog(Self x) {
    if (x.v_<0) x.v_=-x.v_;
    return x;
  }

  Self& poweq(const T& power) {
#if LOGVAL_CHECK_NEG
    if (s_) {
      std::cerr << "poweq(T) not implemented when s_ is true\n";
      std::abort();
    } else
#endif
      v_ *= power;
    return *this;
  }

  //remember, s_ means negative.
  inline bool lt(Self const& o) const {
    return s_==o.s_ ? v_ < o.v_ : s_ > o.s_;
  }
  inline bool gt(Self const& o) const {
    return s_==o.s_ ? o.v_ < v_ : s_ < o.s_;
  }

  Self operator-() const {
    return Self(v_,!s_);
  }
  void negate() { s_ = !s_; }

  Self inverse() const { return Self(-v_,s_); }

  Self pow(const T& power) const {
    Self res = *this;
    res.poweq(power);
    return res;
  }

  Self root(const T& root) const {
    return pow(1/root);
  }

  T as_float() const {
    if (s_) return -std::exp(v_); else return std::exp(v_);
  }

  bool s_;
  T v_;

};

template <class T>
struct semiring_traits<LogVal<T> > : default_semiring_traits<LogVal<T> > {
  static const bool has_logplus=true;
  static const bool has_besteq=true;
  static const bool has_order=true;
  static const bool has_subtract=true;
  static const bool has_negative=true;
};

// copy elision - as opposed to explicit copy of LogVal<T> const& o1, we should be able to construct Logval r=a+(b+c) as a single result in place in r.  todo: return std::move(o1) - C++0x
template<class T>
LogVal<T> operator+(LogVal<T> o1, const LogVal<T>& o2) {
  o1 += o2;
  return o1;
}

template<class T>
LogVal<T> operator*(LogVal<T> o1, const LogVal<T>& o2) {
  o1 *= o2;
  return o1;
}

template<class T>
LogVal<T> operator/(LogVal<T> o1, const LogVal<T>& o2) {
  o1 /= o2;
  return o1;
}

template<class T>
LogVal<T> operator-(LogVal<T> o1, const LogVal<T>& o2) {
  o1 -= o2;
  return o1;
}

template<class T>
T log(const LogVal<T>& o) {
#ifdef LOGVAL_CHECK_NEG
  if (o.s_) return log(-1.0);
#endif
  return o.v_;
}

template<class T>
LogVal<T> abs(const LogVal<T>& o) {
  if (o.s_) {
    LogVal<T> res = o;
    res.s_ = false;
    return res;
  } else { return o; }
}

template <class T>
LogVal<T> pow(const LogVal<T>& b, const T& e) {
  return b.pow(e);
}

template <class T>
bool operator==(const LogVal<T>& lhs, const LogVal<T>& rhs) {
  return (lhs.v_ == rhs.v_) && (lhs.s_ == rhs.s_);
}

template <class T>
bool operator!=(const LogVal<T>& lhs, const LogVal<T>& rhs) {
  return !(lhs == rhs);
}

template <class T>
bool operator<(const LogVal<T>& lhs, const LogVal<T>& rhs) {
  if (lhs.s_ == rhs.s_) {
    return (lhs.v_ < rhs.v_);
  } else {
    return lhs.s_ > rhs.s_;
  }
}

template <class T>
bool operator<=(const LogVal<T>& lhs, const LogVal<T>& rhs) {
  return (lhs < rhs) || (lhs == rhs);
}

template <class T>
bool operator>(const LogVal<T>& lhs, const LogVal<T>& rhs) {
  return !(lhs <= rhs);
}

template <class T>
bool operator>=(const LogVal<T>& lhs, const LogVal<T>& rhs) {
  return !(lhs < rhs);
}

template <class T>
std::size_t hash_value(const LogVal<T>& x) { return x.hash_impl(); }

#endif
