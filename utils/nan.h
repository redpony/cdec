#ifndef NAN_H
#define NAN_H
//TODO: switch to C99 isnan isfinite isinf etc. (faster)

#include <limits>

template <bool> struct nan_static_assert;
template <> struct nan_static_assert<true> { };

// is_iec559 i.e. only IEEE 754 float has x != x <=> x is nan
template<typename T>
inline bool is_nan(T x) {
//    static_cast<void>(sizeof(nan_static_assert<std::numeric_limits<T>::has_quiet_NaN>));
    return std::numeric_limits<T>::has_quiet_NaN && (x != x);
}

template <typename T>
inline bool is_inf(T x) {
//    static_cast<void>(sizeof(nan_static_assert<std::numeric_limits<T>::has_infinity>));
    return x == std::numeric_limits<T>::infinity() || x == -std::numeric_limits<T>::infinity();
}

template <typename T>
inline bool is_pos_inf(T x) {
//    static_cast<void>(sizeof(nan_static_assert<std::numeric_limits<T>::has_infinity>));
    return x == std::numeric_limits<T>::infinity();
}

template <typename T>
inline bool is_neg_inf(T x) {
//    static_cast<void>(sizeof(nan_static_assert<std::numeric_limits<T>::has_infinity>));
    return x == -std::numeric_limits<T>::infinity();
}

//c99 isfinite macro shoudl be much faster
template <typename T>
inline bool is_finite(T x) {
  return !is_nan(x) && !is_inf(x);
}


#endif
