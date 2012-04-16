#ifndef _ALIAS_SAMPLER_H_
#define _ALIAS_SAMPLER_H_

#include <vector>
#include <limits>

// R. A. Kronmal and A. V. Peterson, Jr. (1977) On the alias method for
// generating random variables from a discrete distribution. In The American
// Statistician, Vol. 33, No. 4. Pages 214--218.
//
// Intuition: a multinomial with N outcomes can be rewritten as a uniform
// mixture of N Bernoulli distributions. The ith Bernoulli returns i with
// probability F[i], otherwise it returns an "alias" value L[i]. The
// constructor computes the F's and L's given an arbitrary multionimial p in
// O(n) time and Draw returns samples in O(1) time.
struct AliasSampler {
  AliasSampler() {}
  explicit AliasSampler(const std::vector<double>& p) { Init(p); }
  void Init(const std::vector<double>& p) {
    const unsigned N = p.size();
    cutoffs_.resize(p.size());
    aliases_.clear();
    aliases_.resize(p.size(), std::numeric_limits<unsigned>::max());
    std::vector<unsigned> s,g;
    for (unsigned i = 0; i < N; ++i) {
      const double cutoff = cutoffs_[i] = N * p[i];
      if (cutoff >= 1.0) g.push_back(i); else s.push_back(i);
    }
    while(!s.empty() && !g.empty()) {
      const unsigned k = g.back();
      const unsigned j = s.back();
      aliases_[j] = k;
      cutoffs_[k] -= 1.0 - cutoffs_[j];
      s.pop_back();
      if (cutoffs_[k] < 1.0) {
        g.pop_back();
        s.push_back(k);
      }
    }
  }
  template <typename Uniform01Generator>
  unsigned Draw(Uniform01Generator& u01) const {
    const unsigned n = u01() * cutoffs_.size();
    if (u01() > cutoffs_[n]) return aliases_[n]; else return n;
  }
  std::vector<double> cutoffs_;    // F
  std::vector<unsigned> aliases_;  // L
};

#endif
