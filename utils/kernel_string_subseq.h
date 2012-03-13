#ifndef _KERNEL_STRING_SUBSEQ_H_
#define _KERNEL_STRING_SUBSEQ_H_

#include <vector>
#include <cmath>
#include <boost/multi_array.hpp>

template <unsigned N, typename T>
float ssk(const T* s, const size_t s_size, const T* t, const size_t t_size, const float lambda) {
  assert(N > 0);
  boost::multi_array<float, 3> kp(boost::extents[N + 1][s_size + 1][t_size + 1]);
  const float l2 = lambda * lambda;
  for (unsigned j = 0; j < s_size; ++j)
    for (unsigned k = 0; k < t_size; ++k)
      kp[0][j][k] = 1.0f;
  for (unsigned i = 0; i < N; ++i) {
    for (unsigned j = 0; j < s_size; ++j) {
      float kpp = 0.0f;
      for (unsigned k = 0; k < t_size; ++k) {
        kpp = lambda * (kpp + lambda * (s[j]==t[k]) * kp[i][j][k]);
        kp[i + 1][j + 1][k + 1] = lambda * kp[i + 1][j][k + 1] + kpp;
      }
    }
  }
  float kn = 0.0f;
  for (int i = 0; i < N; ++i)
    for (int j = 0; j < s_size; ++j)
      for (int k = 0; k < t_size; ++k)
        kn += l2 * (s[j] == t[k]) * kp[i][j][k];
  return kn;
}

template <unsigned N, typename T>
float ssk(const std::vector<T>& s, const std::vector<T>& t, const float lambda) {
  float kst = ssk<N, T>(&s[0], s.size(), &t[0], t.size(), lambda);
  if (!kst) return 0.0f;
  float kss = ssk<N, T>(&s[0], s.size(), &s[0], s.size(), lambda);
  float ktt = ssk<N, T>(&t[0], t.size(), &t[0], t.size(), lambda);
  return kst / std::sqrt(kss * ktt);
}

template <unsigned N>
float ssk(const std::string& s, const std::string& t, const float lambda) {
  float kst = ssk<N, char>(&s[0], s.size(), &t[0], t.size(), lambda);
  if (!kst) return 0.0f;
  float kss = ssk<N, char>(&s[0], s.size(), &s[0], s.size(), lambda);
  float ktt = ssk<N, char>(&t[0], t.size(), &t[0], t.size(), lambda);
  return kst / std::sqrt(kss * ktt);
}

#endif
