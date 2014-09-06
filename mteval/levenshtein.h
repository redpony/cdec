#ifndef _LEVENSHTEIN_H_
#define _LEVENSHTEIN_H_

namespace cdec {

template <typename V>
inline unsigned LevenshteinDistance(const V& a, const V& b) {
  const unsigned m = a.size(), n = b.size();
  std::vector<unsigned> edit((m + 1) * 2);
  for (unsigned i = 0; i <= n; i++) {
    for (unsigned j = 0; j <= m; j++) {
      if (i == 0)
        edit[j] = j;
      else if (j == 0)
        edit[(i % 2) * (m + 1)] = i;
      else
        edit[(i % 2) * (m + 1) + j] = std::min(std::min(
                                edit[(i % 2) * (m + 1) + j - 1] + 1,
                                edit[((i - 1) % 2) * (m + 1) + j] + 1),
                                edit[((i - 1) % 2) * (m + 1) + (j - 1)] 
                                    + (a[j - 1] == b[i - 1] ? 0 : 1));
    }
  }
  return edit[(n % 2) * (m + 1) + m];
}

}

#endif
