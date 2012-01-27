#ifndef MAX_PLUS_H_
#define MAX_PLUS_H_

#define MAX_PLUS_ORDER 0
#define MAX_PLUS_DEBUG(x)

// max-plus algebra.  ordering a > b really means that (i.e. default a<b sorting will do worst (closest to 0) first.  so get used to passing predicates like std::greater<MaxPlus<T> > around
// x+y := max{x,y}
// x*y := x+y
// 0 := -inf
// 1 := 0
// additive inverse does not, but mult. does. (inverse()) and x/y := x-y = x+y.inverse()
//WARNING: default order is reversed, on purpose, i.e. a<b means a "better than" b, i.e. log(p_a)>log(p_b).  sorry.  defaults in libs are to order ascending, but we want best first.

#include <boost/functional/hash.hpp>
#include <iostream>
#include <cstdlib>
#include <cmath>
#include <cassert>
#include <limits>
#include "semiring.h"
#include "show.h"
//#include "logval.h"

template <class T>
class MaxPlus {
 public:
  void print(std::ostream &o) const {
    o<<v_;
  }
  PRINT_SELF(MaxPlus<T>)
  template <class O>
  void operator=(O const& o) {
    v_=o.v_;
  }
  template <class O>
  MaxPlus(O const& o) : v_(o.v_) {  }

  typedef MaxPlus<T> Self;
  MaxPlus() : v_(LOGVAL_LOG0) {}
  explicit MaxPlus(double x) : v_(std::log(x)) {}
  MaxPlus(init_1) : v_(0) {  }
  MaxPlus(init_0) : v_(LOGVAL_LOG0) {  }
  MaxPlus(int x) : v_(std::log(x)) {}
  MaxPlus(unsigned x) : v_(std::log(x)) { }
  MaxPlus(double lnx,bool sign) : v_(lnx) { MAX_PLUS_DEBUG(assert(!sign)); }
  MaxPlus(double lnx,init_lnx) : v_(lnx) {}
  static Self exp(T lnx) { return MaxPlus(lnx,false); }

  // maybe the below are faster than == 1 and == 0.  i don't know.
  bool is_1() const { return v_==0; }
  bool is_0() const { return v_==LOGVAL_LOG0; }

  static Self One() { return Self(init_1()); }
  static Self Zero() { return Self(init_0()); }
  static Self e() { return Self(1,false); }
  void logeq(const T& v) { v_ = v; }
  bool signbit() const { return false; }

  Self& logpluseq(const Self& a) {
    if (a.is_0()) return *this;
    if (a.v_ < v_) {
      v_ = v_ + log1p(std::exp(a.v_ - v_));
    } else {
      v_ = a.v_ + log1p(std::exp(v_ - a.v_));
    }
    return *this;
  }

  Self& besteq(const Self& a) {
    if (a.v_ < v_)
      v_=a.v_;
    return *this;
  }

  Self& operator+=(const Self& a) {
    if (a.v_ < v_)
      v_=a.v_;
    return *this;
  }

  Self& operator*=(const Self& a) {
    v_ += a.v_;
    return *this;
  }

  Self& operator/=(const Self& a) {
    v_ -= a.v_;
    return *this;
  }

  // Self(fabs(log(x)),x.s_)
  friend Self abslog(Self x) {
    if (x.v_<0) x.v_=-x.v_;
    return x;
  }

  Self& poweq(const T& power) {
    v_ *= power;
    return *this;
  }

  Self inverse() const {
    return Self(-v_,false);
  }

  Self pow(const T& power) const {
    Self res = *this;
    res.poweq(power);
    return res;
  }

  Self root(const T& root) const {
    return pow(1/root);
  }

// copy elision - as opposed to explicit copy of Self const& o1, we should be able to construct Logval r=a+(b+c) as a single result in place in r.  todo: return std::move(o1) - C++0x
  friend inline Self operator+(Self a,Self const& b) {
    a+=b;
    return a;
  }
  friend inline Self operator*(Self a,Self const& b) {
    a*=b;
    return a;
  }
  friend inline Self operator/(Self a,Self const& b) {
    a/=b;
    return a;
  }
  friend inline T log(Self const& a) {
    return a.v_;
  }
  friend inline T pow(Self const& a,T const& e) {
    return a.pow(e);
  }

  // intentionally not defining an operator < or operator > - because you may want to default (for library convenience) a<b means a better than b (i.e. gt)
  inline bool lt(Self const& o) const {
    return v_ < o.v_;
  }
  inline bool gt(Self const& o) const {
    return o.v_ > v_;
  }
  friend inline bool operator==(Self const& lhs, Self const& rhs) {
    return lhs.v_ == rhs.v_;
  }
  friend inline bool operator!=(Self const& lhs, Self const& rhs) {
    return lhs.v_ != rhs.v_;
  }
  std::size_t hash() const {
    using namespace boost;
    return hash_value(v_);
  }
  friend inline std::size_t hash_value(Self const& x) {
    return x.hash();
  }

/*
  operator T() const {
  return std::exp(v_);
  }
*/
  T as_float() const {
    return std::exp(v_);
  }

  T v_;
};

template <class T>
struct semiring_traits<MaxPlus<T> > : default_semiring_traits<MaxPlus<T> > {
  static const bool has_logplus=true;
  static const bool has_besteq=true;
#if MAX_PLUS_ORDER
  static const bool have_order=true;
#endif
};

#if MAX_PLUS_ORDER
template <class T>
bool operator<(const MaxPlus<T>& lhs, const MaxPlus<T>& rhs) {
  return (lhs.v_ < rhs.v_);
}

template <class T>
bool operator<=(const MaxPlus<T>& lhs, const MaxPlus<T>& rhs) {
  return (lhs.v_ <= rhs.v_);
}

template <class T>
bool operator>(const MaxPlus<T>& lhs, const MaxPlus<T>& rhs) {
  return (lhs.v_ > rhs.v_);
}

template <class T>
bool operator>=(const MaxPlus<T>& lhs, const MaxPlus<T>& rhs) {
  return (lhs.v_ >= rhs.v_);
}
#endif

#endif
