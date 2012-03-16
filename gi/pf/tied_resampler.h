#ifndef _TIED_RESAMPLER_H_
#define _TIED_RESAMPLER_H_

#include <set>
#include <vector>
#include "sampler.h"
#include "slice_sampler.h"
#include "m.h"

template <class CRP>
struct TiedResampler {
  explicit TiedResampler(double da, double db, double ss, double sr, double d=0.5, double s=1.0) :
      d_alpha(da),
      d_beta(db),
      s_shape(ss),
      s_rate(sr),
      discount(d),
      strength(s) {}

  void Add(CRP* crp) {
    crps.insert(crp);
    crp->set_discount(discount);
    crp->set_strength(strength);
    assert(!crp->has_discount_prior());
    assert(!crp->has_strength_prior());
  }

  void Remove(CRP* crp) {
    crps.erase(crp);
  }

  size_t size() const {
    return crps.size();
  }

  double LogLikelihood(double d, double s) const {
    if (s <= -d) return -std::numeric_limits<double>::infinity();
    double llh = Md::log_beta_density(d, d_alpha, d_beta) +
                 Md::log_gamma_density(d + s, s_shape, s_rate);
    for (typename std::set<CRP*>::iterator it = crps.begin(); it != crps.end(); ++it)
      llh += (*it)->log_crp_prob(d, s);
    return llh;
  }

  double LogLikelihood() const {
    return LogLikelihood(discount, strength);
  }

  struct DiscountResampler {
    DiscountResampler(const TiedResampler& m) : m_(m) {}
    const TiedResampler& m_;
    double operator()(const double& proposed_discount) const {
      return m_.LogLikelihood(proposed_discount, m_.strength);
    }
  };

  struct AlphaResampler {
    AlphaResampler(const TiedResampler& m) : m_(m) {}
    const TiedResampler& m_;
    double operator()(const double& proposed_strength) const {
      return m_.LogLikelihood(m_.discount, proposed_strength);
    }
  };

  void ResampleHyperparameters(MT19937* rng, const unsigned nloop = 5, const unsigned niterations = 10) {
    if (size() == 0) { std::cerr << "EMPTY - not resampling\n"; return; }
    const DiscountResampler dr(*this);
    const AlphaResampler ar(*this);
    for (int iter = 0; iter < nloop; ++iter) {
      strength = slice_sampler1d(ar, strength, *rng, -discount + std::numeric_limits<double>::min(),
                              std::numeric_limits<double>::infinity(), 0.0, niterations, 100*niterations);
      double min_discount = std::numeric_limits<double>::min();
      if (strength < 0.0) min_discount -= strength;
      discount = slice_sampler1d(dr, discount, *rng, min_discount,
                          1.0, 0.0, niterations, 100*niterations);
    }
    strength = slice_sampler1d(ar, strength, *rng, -discount + std::numeric_limits<double>::min(),
                            std::numeric_limits<double>::infinity(), 0.0, niterations, 100*niterations);
    std::cerr << "TiedCRPs(d=" << discount << ",s="
              << strength << ") = " << LogLikelihood(discount, strength) << std::endl;
    for (typename std::set<CRP*>::iterator it = crps.begin(); it != crps.end(); ++it)
      (*it)->set_hyperparameters(discount, strength);
  }
 private:
  std::set<CRP*> crps;
  const double d_alpha, d_beta, s_shape, s_rate;
  double discount, strength;
};

// split according to some criterion
template <class CRP>
struct BinTiedResampler {
  explicit BinTiedResampler(unsigned nbins) :
      resamplers(nbins, TiedResampler<CRP>(1,1,1,1)) {}

  void Add(unsigned bin, CRP* crp) {
    resamplers[bin].Add(crp);
  }

  void Remove(unsigned bin, CRP* crp) {
    resamplers[bin].Remove(crp);
  }

  void ResampleHyperparameters(MT19937* rng) {
    for (unsigned i = 0; i < resamplers.size(); ++i) {
      std::cerr << "BIN " << i << " (" << resamplers[i].size() << " CRPs): " << std::flush;
      resamplers[i].ResampleHyperparameters(rng);
    }
  }

  double LogLikelihood() const {
    double llh = 0;
    for (unsigned i = 0; i < resamplers.size(); ++i)
      llh += resamplers[i].LogLikelihood();
    return llh;
  }

 private:
  std::vector<TiedResampler<CRP> > resamplers;
};

#endif
