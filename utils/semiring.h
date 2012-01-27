#ifndef SEMIRING_H
#define SEMIRING_H

struct init_1 {  };
struct init_0 {  };
struct init_minus_1 { };
struct init_lnx {  };

#undef LOGVAL_LOG0
#define LOGVAL_LOG0 -std::numeric_limits<T>::infinity()

/* if T is a semiring,

   then constructors:
   T() := T(init_0)
   T(init_0()) := semiring 0
   T(init_1()) := semiring 1
   (copy constructor, equality, assignment also)
   T(other initializers) - semiring dependent e.g. T(double lnx,bool is_negative); T(double lnx,init_lnx); T(double x) := T(log(fabs(x)),signbit(x))

   optional: T(init_minus_1()) := -1, assuming additive inverse

   if T a,b;
   then a+b := semiring + - commutative monoid with 0
   a*b := semiring * with identity 1 - monoid with 1
   * distributes over +
   0*a=a*0=0

   also a+= a*=

   optional: commutative multiplication (commutative semiring)

   non-semiring concepts you may also have:
   order (a<b)
   logplus(a,b), logpluseq(a,b), T::exp(lnx), log(a)
   a-b, usually with a-b := a+T(init_minus_1())*b
   a.inverse() : a*a.inverse() = 1
   a.left_inverse() : a.left_inverse()*a = 1 (define if not commuatative and it exists)
   signbit(a) := a<0
   pow(a,2) := a*a
   pow(a,2.3)
*/

template <class T>
struct default_semiring_traits {
  static const T One;
  static const T Zero;
  static inline bool is_1(T const& x) { return x==One; }
  static inline bool is_0(T const& x) { return x==Zero; }
  static const bool commutative=true;
  static const bool has_inverse=true; // multiplicative, that is
//  static const bool has_left_inverse=false; // note: not intended to imply that inverse isn't a left_inverse
  static const bool has_order=false; // we usually don't defined < > <= >= although == != must exist
  static const bool has_ltgt=true; // but we will probably give a a.lt(b) x.gt(y) which are a<b and x>y resp.
  static const bool has_subtract=false; // e.g. tropical can't do this
  static const bool has_logplus=false;
  static const bool has_pow=true;
  static const bool has_init_float=true; // implies you can T a; T a.as_float() as well as T(float f)
  static const bool has_negative=false;
  static const bool has_ostream=false; // we could provide these but for some reason we usually don't
  static const bool has_istream=false;
};

template <class T>
struct semiring_traits : default_semiring_traits<T> {
};


template <class T>
const T default_semiring_traits<T>::One=init_1();
template <class T>
const T default_semiring_traits<T>::Zero=init_0();

#endif
