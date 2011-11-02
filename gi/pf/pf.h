#ifndef _PF_H_
#define _PF_H_

#include <cassert>
#include <vector>
#include "sampler.h"
#include "prob.h"

template <typename ParticleType>
struct ParticleRenormalizer {
  void operator()(std::vector<ParticleType>* pv) const {
    if (pv->empty()) return;
    prob_t z = prob_t::Zero();
    for (unsigned i = 0; i < pv->size(); ++i)
      z += (*pv)[i].weight;
    assert(z > prob_t::Zero());
    for (unsigned i = 0; i < pv->size(); ++i)
      (*pv)[i].weight /= z;
  }
};

template <typename ParticleType>
struct MultinomialResampleFilter {
  explicit MultinomialResampleFilter(MT19937* rng) : rng_(rng) {}

  void operator()(std::vector<ParticleType>* pv) {
    if (pv->empty()) return;
    std::vector<ParticleType>& ps = *pv;
    SampleSet<prob_t> ss;
    for (int i = 0; i < ps.size(); ++i)
      ss.add(ps[i].weight);
    std::vector<ParticleType> nps; nps.reserve(ps.size());
    const prob_t uniform_weight(1.0 / ps.size());
    for (int i = 0; i < ps.size(); ++i) {
      nps.push_back(ps[rng_->SelectSample(ss)]);
      nps[i].weight = uniform_weight;
    }
    nps.swap(ps);
  }

 private:
  MT19937* rng_;
};

template <typename ParticleType>
struct SystematicResampleFilter {
  explicit SystematicResampleFilter(MT19937* rng) : rng_(rng), renorm_() {}

  void operator()(std::vector<ParticleType>* pv) {
    if (pv->empty()) return;
    renorm_(pv);
    std::vector<ParticleType>& ps = *pv;
    std::vector<ParticleType> nps; nps.reserve(ps.size());
    double lower = 0, upper = 0;
    const double skip = 1.0 / ps.size();
    double u_j = rng_->next() * skip;
    //std::cerr << "u_0: " << u_j << std::endl;
    int j = 0;
    for (unsigned i = 0; i < ps.size(); ++i) {
      upper += ps[i].weight.as_float();
      //std::cerr << "lower: " << lower << " upper: " << upper << std::endl;
      // how many children does ps[i] have?
      while (u_j < lower) { u_j += skip; ++j; }
      while (u_j >= lower && u_j <= upper) {
        assert(j < ps.size());
        nps.push_back(ps[i]);
        u_j += skip;
        //std::cerr << " add u_j=" << u_j << std::endl;
        ++j;
      }
      lower = upper;
    }
    //std::cerr << ps.size() << " " << nps.size() << "\n";
    assert(ps.size() == nps.size());
    //exit(1);
    ps.swap(nps);
  }

 private:
  MT19937* rng_;
  ParticleRenormalizer<ParticleType> renorm_;
};

#endif
