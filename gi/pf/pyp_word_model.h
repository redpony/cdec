#ifndef _PYP_WORD_MODEL_H_
#define _PYP_WORD_MODEL_H_

#include <iostream>
#include <cmath>
#include <vector>
#include "prob.h"
#include "ccrp.h"
#include "m.h"
#include "tdict.h"
#include "os_phrase.h"

// PYP(d,s,poisson-uniform) represented as a CRP
template <class Base>
struct PYPWordModel {
  explicit PYPWordModel(Base* b) :
      base(*b),
      r(1,1,1,1,0.66,50.0)
    {}

  void ResampleHyperparameters(MT19937* rng) {
    r.resample_hyperparameters(rng);
    std::cerr << " PYPWordModel(d=" << r.discount() << ",s=" << r.strength() << ")\n";
  }

  inline prob_t operator()(const std::vector<WordID>& s) const {
    return r.prob(s, base(s));
  }

  inline void Increment(const std::vector<WordID>& s, MT19937* rng) {
    if (r.increment(s, base(s), rng))
      base.Increment(s, rng);
  }

  inline void Decrement(const std::vector<WordID>& s, MT19937 *rng) {
    if (r.decrement(s, rng))
      base.Decrement(s, rng);
  }

  inline prob_t Likelihood() const {
    prob_t p; p.logeq(r.log_crp_prob());
    p *= base.Likelihood();
    return p;
  }

  void Summary() const {
    std::cerr << "PYPWordModel: generations=" << r.num_customers()
         << " PYP(d=" << r.discount() << ",s=" << r.strength() << ')' << std::endl;
    for (typename CCRP<std::vector<WordID> >::const_iterator it = r.begin(); it != r.end(); ++it) {
      std::cerr << "   " << it->second.total_dish_count_
                << " (on " << it->second.table_counts_.size() << " tables) "
                << TD::GetString(it->first) << std::endl;
    }
  }

 private:

  Base& base;  // keeps track of the draws from the base distribution
  CCRP<std::vector<WordID> > r;
};

#endif
