#ifndef _QUASI_MODEL2_H_
#define _QUASI_MODEL2_H_

#include <vector>
#include <cmath>
#include <tr1/unordered_map>
#include "boost/functional.hpp"
#include "prob.h"
#include "array2d.h"
#include "slice_sampler.h"
#include "m.h"
#include "have_64_bits.h"

struct AlignmentObservation {
  AlignmentObservation() : src_len(), trg_len(), j(), a_j() {}
  AlignmentObservation(unsigned sl, unsigned tl, unsigned tw, unsigned sw) :
      src_len(sl), trg_len(tl), j(tw), a_j(sw) {}
  unsigned short src_len;
  unsigned short trg_len;
  unsigned short j;
  unsigned short a_j;
};

#ifdef HAVE_64_BITS
inline size_t hash_value(const AlignmentObservation& o) {
  return reinterpret_cast<const size_t&>(o);
}
inline bool operator==(const AlignmentObservation& a, const AlignmentObservation& b) {
  return hash_value(a) == hash_value(b);
}
#else
inline size_t hash_value(const AlignmentObservation& o) {
  size_t h = 1;
  boost::hash_combine(h, o.src_len);
  boost::hash_combine(h, o.trg_len);
  boost::hash_combine(h, o.j);
  boost::hash_combine(h, o.a_j);
  return h;
}
#endif

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

  struct PNullResampler {
    PNullResampler(const QuasiModel2& m) : m_(m) {}
    const QuasiModel2& m_;
    double operator()(const double& proposed_pnull) const {
      return log(m_.Likelihood(m_.alpha_, proposed_pnull));
    }
  };

  struct AlphaResampler {
    AlphaResampler(const QuasiModel2& m) : m_(m) {}
    const QuasiModel2& m_;
    double operator()(const double& proposed_alpha) const {
      return log(m_.Likelihood(proposed_alpha, m_.pnull_.as_float()));
    }
  };

  void ResampleHyperparameters(MT19937* rng, const unsigned nloop = 5, const unsigned niterations = 10) {
    const PNullResampler dr(*this);
    const AlphaResampler ar(*this);
    for (unsigned i = 0; i < nloop; ++i) {
      double pnull = slice_sampler1d(dr, pnull_.as_float(), *rng, 0.00000001,
                            1.0, 0.0, niterations, 100*niterations);
      pnull_ = prob_t(pnull);
      alpha_ = slice_sampler1d(ar, alpha_, *rng, 0.00000001,
                              std::numeric_limits<double>::infinity(), 0.0, niterations, 100*niterations);
    }
    std::cerr << "QuasiModel2(alpha=" << alpha_ << ",p_null="
              << pnull_.as_float() << ") = " << Likelihood() << std::endl;
    zcache_.clear();
  }

  prob_t Likelihood() const {
    return Likelihood(alpha_, pnull_.as_float());
  }

  prob_t Likelihood(double alpha, double ppnull) const {
    const prob_t pnull(ppnull);
    const prob_t pnotnull(1 - ppnull);

    prob_t p;
    p.logeq(Md::log_gamma_density(alpha, 0.1, 25));  // TODO configure
    assert(!p.is_0());
    prob_t prob_of_ppnull; prob_of_ppnull.logeq(Md::log_beta_density(ppnull, 2, 10));
    assert(!prob_of_ppnull.is_0());
    p *= prob_of_ppnull;
    for (ObsCount::const_iterator it = obs_.begin(); it != obs_.end(); ++it) {
      const AlignmentObservation& ao = it->first;
      if (ao.a_j) {
        prob_t u = XUnnormalizedProb(ao.a_j, ao.j, ao.src_len, ao.trg_len, alpha);
        prob_t z = XComputeZ(ao.j, ao.src_len, ao.trg_len, alpha);
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
  static prob_t XUnnormalizedProb(unsigned a_j, unsigned j, unsigned src_len, unsigned trg_len, double alpha) {
    prob_t p;
    p.logeq(-fabs(double(a_j - 1) / src_len - double(j) / trg_len) * alpha);
    return p;
  }

  static prob_t XComputeZ(unsigned j, unsigned src_len, unsigned trg_len, double alpha) {
    prob_t z = prob_t::Zero();
    for (int a_j = 1; a_j <= src_len; ++a_j)
      z += XUnnormalizedProb(a_j, j, src_len, trg_len, alpha);
    return z;
  }

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
