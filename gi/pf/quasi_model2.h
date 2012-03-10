#ifndef _QUASI_MODEL2_H_
#define _QUASI_MODEL2_H_

#include <vector>
#include <cmath>
#include <tr1/unordered_map>
#include "boost/functional.hpp"
#include "prob.h"
#include "array2d.h"

struct AlignmentObservation {
  AlignmentObservation() : src_len(), trg_len(), j(), a_j() {}
  AlignmentObservation(unsigned sl, unsigned tl, unsigned tw, unsigned sw) :
      src_len(sl), trg_len(tl), j(tw), a_j(sw) {}
  unsigned short src_len;
  unsigned short trg_len;
  unsigned short j;
  unsigned short a_j;
};

inline size_t hash_value(const AlignmentObservation& o) {
  return reinterpret_cast<const size_t&>(o);
}

inline bool operator==(const AlignmentObservation& a, const AlignmentObservation& b) {
  return hash_value(a) == hash_value(b);
}

struct QuasiModel2 {
  explicit QuasiModel2(double alpha, double pnull = 0.1) :
      alpha_(alpha),
      pnull_(pnull),
      pnotnull_(1 - pnull) {}

  // a_j = 0 => NULL; src_len does *not* include null
  prob_t Prob(unsigned a_j, unsigned j, unsigned src_len, unsigned trg_len) const {
    if (!a_j) return pnull_;
    return pnotnull_ *
       prob_t(UnnormalizedProb(a_j, j, src_len, trg_len, alpha_) / GetOrComputeZ(j, src_len, trg_len));
  }

  void Increment(unsigned a_j, unsigned j, unsigned src_len, unsigned trg_len) {
    assert(a_j <= src_len);
    assert(j < trg_len);
    ++obs_[AlignmentObservation(src_len, trg_len, j, a_j)];
  }

  void Decrement(unsigned a_j, unsigned j, unsigned src_len, unsigned trg_len) {
    const AlignmentObservation ao(src_len, trg_len, j, a_j);
    int &cc = obs_[ao];
    assert(cc > 0);
    --cc;
    if (!cc) obs_.erase(ao);
  }

  prob_t Likelihood() const {
    return Likelihood(alpha_, pnull_.as_float());
  }

  prob_t Likelihood(double alpha, double ppnull) const {
    const prob_t pnull(ppnull);
    const prob_t pnotnull(1 - ppnull);

    prob_t p = prob_t::One();
    for (ObsCount::const_iterator it = obs_.begin(); it != obs_.end(); ++it) {
      const AlignmentObservation& ao = it->first;
      if (ao.a_j) {
        double u = UnnormalizedProb(ao.a_j, ao.j, ao.src_len, ao.trg_len, alpha);
        double z = ComputeZ(ao.j, ao.src_len, ao.trg_len, alpha);
        prob_t pa(u / z);
        pa *= pnotnull;
        pa.poweq(it->second);
        p *= pa;
      } else {
        p *= pnull.pow(it->second);
      }
    }
    return p;
  }

 private:
  static double UnnormalizedProb(unsigned a_j, unsigned j, unsigned src_len, unsigned trg_len, double alpha) {
    return exp(-fabs(double(a_j - 1) / src_len - double(j) / trg_len) * alpha);
  }

  static double ComputeZ(unsigned j, unsigned src_len, unsigned trg_len, double alpha) {
    double z = 0;
    for (int a_j = 1; a_j <= src_len; ++a_j)
      z += UnnormalizedProb(a_j, j, src_len, trg_len, alpha);
    return z;
  }

  const double& GetOrComputeZ(unsigned j, unsigned src_len, unsigned trg_len) const {
    if (src_len >= zcache_.size())
      zcache_.resize(src_len + 1);
    if (trg_len >= zcache_[src_len].size())
      zcache_[src_len].resize(trg_len + 1);
    std::vector<double>& zv = zcache_[src_len][trg_len];
    if (zv.size() == 0)
      zv.resize(trg_len);
    double& z = zv[j];
    if (!z)
      z = ComputeZ(j, src_len, trg_len, alpha_);
    return z;
  }

  double alpha_;
  prob_t pnull_;
  prob_t pnotnull_;
  mutable std::vector<std::vector<std::vector<double> > > zcache_;
  typedef std::tr1::unordered_map<AlignmentObservation, int, boost::hash<AlignmentObservation> > ObsCount;
  ObsCount obs_;
};

#endif
