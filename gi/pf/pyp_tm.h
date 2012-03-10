#ifndef PYP_LEX_TRANS
#define PYP_LEX_TRANS

#include <vector>
#include "wordid.h"
#include "prob.h"
#include "sampler.h"

struct TRule;
struct PYPWordModel;
template <typename T> struct ConditionalPYPWordModel;

struct PYPLexicalTranslation {
  explicit PYPLexicalTranslation(const std::vector<std::vector<WordID> >& lets,
                                 const unsigned num_letters);

  prob_t Likelihood() const;

  void ResampleHyperparameters(MT19937* rng);
  prob_t Prob(WordID src, WordID trg) const;  // return p(trg | src)
  void Summary() const;
  void Increment(WordID src, WordID trg, MT19937* rng);
  void Decrement(WordID src, WordID trg, MT19937* rng);
  unsigned UniqueConditioningContexts() const;

 private:
  const std::vector<std::vector<WordID> >& letters;   // spelling dictionary
  PYPWordModel* up0;  // base distribuction (model English word)
  ConditionalPYPWordModel<PYPWordModel>* tmodel;  // translation distributions
                      // (model English word | French word)
  const WordID kX;
};

#endif
