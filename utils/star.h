#ifndef _STAR_H_
#define _STAR_H_

// star(x) computes the infinite sum x^0 + x^1 + x^2 + ...

template <typename T>
inline T star(const T& x) {
  if (!x) return T();
  if (x > T(1)) return std::numeric_limits<T>::infinity();
  if (x < -T(1)) return -std::numeric_limits<T>::infinity();
  return T(1) / (T(1) - x);
}

inline bool star(bool x) {
  return x;
}

#endif
