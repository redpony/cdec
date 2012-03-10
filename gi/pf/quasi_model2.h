#ifndef _QUASI_MODEL2_H_
#define _QUASI_MODEL2_H_

#include <vector>
#include <cmath>
#include "prob.h"
#include "array2d.h"

struct QuasiModel2 {
  explicit QuasiModel2(double alpha, double pnull = 0.1) :
      alpha_(alpha),
      pnull_(pnull),
      pnotnull_(1 - pnull),
      z_(1000,1000) {}
  // a_j = 0 => NULL; src_len does *not* include null
  prob_t Pa_j(unsigned a_j, unsigned j, unsigned src_len, unsigned trg_len) const {
    if (!a_j) return pnull_;
    std::vector<prob_t>& zv = z_(src_len, trg_len);
    if (zv.size() == 0)
      zv.resize(trg_len);
    
    prob_t& z = zv[j];
    if (z.is_0()) z = ComputeZ(j, src_len, trg_len);

    prob_t p;
    p.logeq(-fabs(double(a_j - 1) / src_len - double(j) / trg_len) * alpha_);
    p *= pnotnull_;
    p /= z;
    return p;
  }
 private:
  prob_t ComputeZ(unsigned j, unsigned src_len, unsigned trg_len) const {
    prob_t p, z = prob_t::Zero();
    for (int a_j = 1; a_j <= src_len; ++a_j) {
      p.logeq(-fabs(double(a_j - 1) / src_len - double(j) / trg_len) * alpha_);
      z += p;
    }
    return z;
  }
  double alpha_;
  const prob_t pnull_;
  const prob_t pnotnull_;
  mutable Array2D<std::vector<prob_t> > z_;
};

#endif
