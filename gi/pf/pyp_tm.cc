#include "pyp_tm.h"

#include <tr1/unordered_map>
#include <iostream>
#include <queue>

#include "base_distributions.h"
#include "monotonic_pseg.h"
#include "conditional_pseg.h"
#include "tdict.h"
#include "ccrp.h"
#include "pyp_word_model.h"

#include "tied_resampler.h"

using namespace std;
using namespace std::tr1;

template <typename Base>
struct ConditionalPYPWordModel {
  ConditionalPYPWordModel(Base* b) : base(*b) {}

  void Summary() const {
    cerr << "Number of conditioning contexts: " << r.size() << endl;
    for (RuleModelHash::const_iterator it = r.begin(); it != r.end(); ++it) {
      cerr << TD::Convert(it->first) << "   \tPYP(d=" << it->second.discount() << ",s=" << it->second.strength() << ") --------------------------" << endl;
      for (CCRP<vector<WordID> >::const_iterator i2 = it->second.begin(); i2 != it->second.end(); ++i2)
        cerr << "   " << i2->second.total_dish_count_ << '\t' << TD::GetString(i2->first) << endl;
    }
  }

  void ResampleHyperparameters(MT19937* rng) {
    for (RuleModelHash::iterator it = r.begin(); it != r.end(); ++it)
      it->second.resample_hyperparameters(rng);
  } 

  prob_t Prob(const WordID src, const vector<WordID>& trglets) const {
    RuleModelHash::const_iterator it = r.find(src);
    if (it == r.end()) {
      return base(trglets);
    } else {
      return it->second.prob(trglets, base(trglets));
    }
  }

  void Increment(const WordID src, const vector<WordID>& trglets, MT19937* rng) {
    RuleModelHash::iterator it = r.find(src);
    if (it == r.end())
      it = r.insert(make_pair(src, CCRP<vector<WordID> >(1,1,1,1,0.5,1.0))).first;
    if (it->second.increment(trglets, base(trglets), rng))
      base.Increment(trglets, rng);
  }

  void Decrement(const WordID src, const vector<WordID>& trglets, MT19937* rng) {
    RuleModelHash::iterator it = r.find(src);
    assert(it != r.end());
    if (it->second.decrement(trglets, rng)) {
      base.Decrement(trglets, rng);
    }
  }

  prob_t Likelihood() const {
    prob_t p = prob_t::One();
    for (RuleModelHash::const_iterator it = r.begin(); it != r.end(); ++it) {
      prob_t q; q.logeq(it->second.log_crp_prob());
      p *= q;
    }
    return p;
  }

  unsigned UniqueConditioningContexts() const {
    return r.size();
  }

  Base& base;
  typedef unordered_map<WordID, CCRP<vector<WordID> > > RuleModelHash;
  RuleModelHash r;
};

PYPLexicalTranslation::PYPLexicalTranslation(const vector<vector<WordID> >& lets,
                                             const unsigned num_letters) :
    letters(lets),
    up0(new PYPWordModel(num_letters)),
    tmodel(new ConditionalPYPWordModel<PYPWordModel>(up0)),
    kX(-TD::Convert("X")) {}

void PYPLexicalTranslation::Summary() const {
  tmodel->Summary();
  up0->Summary();
}

prob_t PYPLexicalTranslation::Likelihood() const {
  prob_t p = up0->Likelihood();
  p *= tmodel->Likelihood();
  return p;
}

void PYPLexicalTranslation::ResampleHyperparameters(MT19937* rng) {
  tmodel->ResampleHyperparameters(rng);
  up0->ResampleHyperparameters(rng);
}

unsigned PYPLexicalTranslation::UniqueConditioningContexts() const {
  return tmodel->UniqueConditioningContexts();
}

prob_t PYPLexicalTranslation::Prob(WordID src, WordID trg) const {
  return tmodel->Prob(src, letters[trg]);
}

void PYPLexicalTranslation::Increment(WordID src, WordID trg, MT19937* rng) {
  tmodel->Increment(src, letters[trg], rng);
}

void PYPLexicalTranslation::Decrement(WordID src, WordID trg, MT19937* rng) {
  tmodel->Decrement(src, letters[trg], rng);
}

