#include "hpyp_tm.h"

#include <tr1/unordered_map>
#include <iostream>
#include <queue>

#include "tdict.h"
#include "ccrp.h"
#include "pyp_word_model.h"
#include "tied_resampler.h"

using namespace std;
using namespace std::tr1;

struct FreqBinner {
  FreqBinner(const std::string& fname) { fd_.Load(fname); }
  unsigned NumberOfBins() const { return fd_.Max() + 1; }
  unsigned Bin(const WordID& w) const { return fd_.LookUp(w); }
  FreqDict<unsigned> fd_;
};

template <typename Base, class Binner = FreqBinner>
struct ConditionalPYPWordModel {
  ConditionalPYPWordModel(Base* b, const Binner* bnr = NULL) :
      base(*b),
      binner(bnr),
      btr(binner ? binner->NumberOfBins() + 1u : 2u) {}

  void Summary() const {
    cerr << "Number of conditioning contexts: " << r.size() << endl;
    for (RuleModelHash::const_iterator it = r.begin(); it != r.end(); ++it) {
      cerr << TD::Convert(it->first) << "   \tPYP(d=" << it->second.discount() << ",s=" << it->second.strength() << ") --------------------------" << endl;
      for (CCRP<vector<WordID> >::const_iterator i2 = it->second.begin(); i2 != it->second.end(); ++i2)
        cerr << "   " << i2->second.total_dish_count_ << '\t' << TD::GetString(i2->first) << endl;
    }
  }

  void ResampleHyperparameters(MT19937* rng) {
    btr.ResampleHyperparameters(rng);
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
    if (it == r.end()) {
      it = r.insert(make_pair(src, CCRP<vector<WordID> >(0.5,1.0))).first;
      static const WordID kNULL = TD::Convert("NULL");
      unsigned bin = (src == kNULL ? 0 : 1);
      if (binner && bin) { bin = binner->Bin(src) + 1; }
      btr.Add(bin, &it->second);
    }
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

  // TODO tie PYP hyperparameters based on source word frequency bins
  Base& base;
  const Binner* binner;
  BinTiedResampler<CCRP<vector<WordID> > > btr;
  typedef unordered_map<WordID, CCRP<vector<WordID> > > RuleModelHash;
  RuleModelHash r;
};

HPYPLexicalTranslation::HPYPLexicalTranslation(const vector<vector<WordID> >& lets,
                                               const unsigned vocab_size,
                                               const unsigned num_letters) :
    letters(lets),
    base(vocab_size, num_letters, 5),
    up0(new PYPWordModel<PoissonUniformWordModel>(&base)),
    tmodel(new ConditionalPYPWordModel<PYPWordModel<PoissonUniformWordModel> >(up0, new FreqBinner("10k.freq"))),
    kX(-TD::Convert("X")) {}

void HPYPLexicalTranslation::Summary() const {
  tmodel->Summary();
  up0->Summary();
}

prob_t HPYPLexicalTranslation::Likelihood() const {
  prob_t p = up0->Likelihood();
  p *= tmodel->Likelihood();
  return p;
}

void HPYPLexicalTranslation::ResampleHyperparameters(MT19937* rng) {
  tmodel->ResampleHyperparameters(rng);
  up0->ResampleHyperparameters(rng);
}

unsigned HPYPLexicalTranslation::UniqueConditioningContexts() const {
  return tmodel->UniqueConditioningContexts();
}

prob_t HPYPLexicalTranslation::Prob(WordID src, WordID trg) const {
  return tmodel->Prob(src, letters[trg]);
}

void HPYPLexicalTranslation::Increment(WordID src, WordID trg, MT19937* rng) {
  tmodel->Increment(src, letters[trg], rng);
}

void HPYPLexicalTranslation::Decrement(WordID src, WordID trg, MT19937* rng) {
  tmodel->Decrement(src, letters[trg], rng);
}

