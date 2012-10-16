#ifndef _EXP_SEMIRING_H_
#define _EXP_SEMIRING_H_

#include <iostream>

// this file implements the first-order expectation semiring described
// in Li & Eisner (EMNLP 2009)

// requirements:
//   RType * RType ==> RType
//   PType * PType ==> PType
//   RType * PType ==> RType
// good examples:
//   PType scalar, RType vector
// BAD examples:
//   PType vector, RType scalar
template <class PType, class RType>
struct PRPair {
  PRPair() : p(), r() {}
  // Inside algorithm requires that T(0) and T(1)
  // return the 0 and 1 values of the semiring
  explicit PRPair(double x) : p(x), r() {}
  PRPair(const PType& p, const RType& r) : p(p), r(r) {}
  PRPair& operator+=(const PRPair& o) {
    p += o.p;
    r += o.r;
    return *this;
  }
  PRPair& operator*=(const PRPair& o) {
    r = (o.r * p) + (o.p * r);
    p *= o.p;
    return *this;
  }
  PType p;
  RType r;
};

template <class P, class R>
std::ostream& operator<<(std::ostream& o, const PRPair<P,R>& x) {
  return o << '<' << x.p << ", " << x.r << '>';
}

template <class P, class R>
const PRPair<P,R> operator+(const PRPair<P,R>& a, const PRPair<P,R>& b) {
  PRPair<P,R> result = a;
  result += b;
  return result;
}

template <class P, class R>
const PRPair<P,R> operator*(const PRPair<P,R>& a, const PRPair<P,R>& b) {
  PRPair<P,R> result = a;
  result *= b;
  return result;
}

template <class P, class PWeightFunction, class R, class RWeightFunction>
struct PRWeightFunction {
  explicit PRWeightFunction(const PWeightFunction& pwf = PWeightFunction(),
                            const RWeightFunction& rwf = RWeightFunction()) :
    pweight(pwf), rweight(rwf) {}
  PRPair<P,R> operator()(const HG::Edge& e) const {
    const P p = pweight(e);
    const R r = rweight(e);
    return PRPair<P,R>(p, r * p);
  }
  const PWeightFunction pweight;
  const RWeightFunction rweight;
};

#endif
