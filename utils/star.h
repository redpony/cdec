#ifndef _STAR_H_
#define _STAR_H_

template <typename T>
T star(const T& x) {
  if (!x) return T();
  if (x > T(1)) return std::numeric_limits<T>::infinity();
  if (x < -T(1)) return -std::numeric_limits<T>::infinity();
  return T(1) / (T(1) - x);
}

#endif
