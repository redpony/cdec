#ifndef _BASE_MEASURES_H_
#define _BASE_MEASURES_H_

#include <vector>
#include <map>
#include <string>
#include <cmath>
#include <iostream>
#include <cassert>

#include "unigrams.h"
#include "trule.h"
#include "prob.h"
#include "tdict.h"
#include "sampler.h"
#include "m.h"
#include "os_phrase.h"

struct Model1 {
  explicit Model1(const std::string& fname) :
      kNULL(TD::Convert("<eps>")),
      kZERO() {
    LoadModel1(fname);
  }

  void LoadModel1(const std::string& fname);

  // returns prob 0 if src or trg is not found
  const prob_t& operator()(WordID src, WordID trg) const {
    if (src == 0) src = kNULL;
    if (src < ttable.size()) {
      const std::map<WordID, prob_t>& cpd = ttable[src];
      const std::map<WordID, prob_t>::const_iterator it = cpd.find(trg);
      if (it != cpd.end())
        return it->second;
    }
    return kZERO;
  }

  const WordID kNULL;
  const prob_t kZERO;
  std::vector<std::map<WordID, prob_t> > ttable;
};

struct PoissonUniformUninformativeBase {
  explicit PoissonUniformUninformativeBase(const unsigned ves) : kUNIFORM(1.0 / ves) {}
  prob_t operator()(const TRule& r) const {
    prob_t p; p.logeq(Md::log_poisson(r.e_.size(), 1.0));
    prob_t q = kUNIFORM; q.poweq(r.e_.size());
    p *= q;
    return p;
  }
  void Summary() const {}
  void ResampleHyperparameters(MT19937*) {}
  void Increment(const TRule&) {}
  void Decrement(const TRule&) {}
  prob_t Likelihood() const { return prob_t::One(); }
  const prob_t kUNIFORM;
};

struct CompletelyUniformBase {
  explicit CompletelyUniformBase(const unsigned ves) : kUNIFORM(1.0 / ves) {}
  prob_t operator()(const TRule&) const {
    return kUNIFORM;
  }
  void Summary() const {}
  void ResampleHyperparameters(MT19937*) {}
  void Increment(const TRule&) {}
  void Decrement(const TRule&) {}
  prob_t Likelihood() const { return prob_t::One(); }
  const prob_t kUNIFORM;
};

struct UnigramWordBase {
  explicit UnigramWordBase(const std::string& fname) : un(fname) {}
  prob_t operator()(const TRule& r) const {
    return un(r.e_);
  }
  const UnigramWordModel un;
};

struct RuleHasher {
  size_t operator()(const TRule& r) const {
    return hash_value(r);
  }
};

struct TableLookupBase {
  TableLookupBase(const std::string& fname);

  prob_t operator()(const TRule& rule) const {
    const std::tr1::unordered_map<TRule,prob_t>::const_iterator it = table.find(rule);
    if (it == table.end()) {
      std::cerr << rule << " not found\n";
      abort();
    }
    return it->second;
  }

  void ResampleHyperparameters(MT19937*) {}
  void Increment(const TRule&) {}
  void Decrement(const TRule&) {}
  prob_t Likelihood() const { return prob_t::One(); }
  void Summary() const {}

  std::tr1::unordered_map<TRule,prob_t,RuleHasher> table;
};

struct PhraseConditionalUninformativeBase {
  explicit PhraseConditionalUninformativeBase(const unsigned vocab_e_size) :
      kUNIFORM_TARGET(1.0 / vocab_e_size) {
    assert(vocab_e_size > 0);
  }

  // return p0 of rule.e_ | rule.f_
  prob_t operator()(const TRule& rule) const {
    return p0(rule.f_, rule.e_, 0, 0);
  }

  prob_t p0(const std::vector<WordID>& vsrc, const std::vector<WordID>& vtrg, int start_src, int start_trg) const;

  void Summary() const {}
  void ResampleHyperparameters(MT19937*) {}
  void Increment(const TRule&) {}
  void Decrement(const TRule&) {}
  prob_t Likelihood() const { return prob_t::One(); }
  const prob_t kUNIFORM_TARGET;
};

struct PhraseConditionalUninformativeUnigramBase {
  explicit PhraseConditionalUninformativeUnigramBase(const std::string& file, const unsigned vocab_e_size) : u(file, vocab_e_size) {}

  // return p0 of rule.e_ | rule.f_
  prob_t operator()(const TRule& rule) const {
    return p0(rule.f_, rule.e_, 0, 0);
  }

  prob_t p0(const std::vector<WordID>& vsrc, const std::vector<WordID>& vtrg, int start_src, int start_trg) const;

  const UnigramModel u;
};

struct PhraseConditionalBase {
  explicit PhraseConditionalBase(const Model1& m1, const double m1mixture, const unsigned vocab_e_size) :
      model1(m1),
      kM1MIXTURE(m1mixture),
      kUNIFORM_MIXTURE(1.0 - m1mixture),
      kUNIFORM_TARGET(1.0 / vocab_e_size) {
    assert(m1mixture >= 0.0 && m1mixture <= 1.0);
    assert(vocab_e_size > 0);
  }

  // return p0 of rule.e_ | rule.f_
  prob_t operator()(const TRule& rule) const {
    return p0(rule.f_, rule.e_, 0, 0);
  }

  prob_t p0(const std::vector<WordID>& vsrc, const std::vector<WordID>& vtrg, int start_src, int start_trg) const;

  const Model1& model1;
  const prob_t kM1MIXTURE;  // Model 1 mixture component
  const prob_t kUNIFORM_MIXTURE; // uniform mixture component
  const prob_t kUNIFORM_TARGET;
};

struct PhraseJointBase {
  explicit PhraseJointBase(const Model1& m1, const double m1mixture, const unsigned vocab_e_size, const unsigned vocab_f_size) :
      model1(m1),
      kM1MIXTURE(m1mixture),
      kUNIFORM_MIXTURE(1.0 - m1mixture),
      kUNIFORM_SOURCE(1.0 / vocab_f_size),
      kUNIFORM_TARGET(1.0 / vocab_e_size) {
    assert(m1mixture >= 0.0 && m1mixture <= 1.0);
    assert(vocab_e_size > 0);
  }

  // return p0 of rule.e_ , rule.f_
  prob_t operator()(const TRule& rule) const {
    return p0(rule.f_, rule.e_, 0, 0);
  }

  prob_t p0(const std::vector<WordID>& vsrc, const std::vector<WordID>& vtrg, int start_src, int start_trg) const;

  const Model1& model1;
  const prob_t kM1MIXTURE;  // Model 1 mixture component
  const prob_t kUNIFORM_MIXTURE; // uniform mixture component
  const prob_t kUNIFORM_SOURCE;
  const prob_t kUNIFORM_TARGET;
};

struct PhraseJointBase_BiDir {
  explicit PhraseJointBase_BiDir(const Model1& m1,
                                 const Model1& im1,
                                 const double m1mixture,
                                 const unsigned vocab_e_size,
                                 const unsigned vocab_f_size) :
      model1(m1),
      invmodel1(im1),
      kM1MIXTURE(m1mixture),
      kUNIFORM_MIXTURE(1.0 - m1mixture),
      kUNIFORM_SOURCE(1.0 / vocab_f_size),
      kUNIFORM_TARGET(1.0 / vocab_e_size) {
    assert(m1mixture >= 0.0 && m1mixture <= 1.0);
    assert(vocab_e_size > 0);
  }

  // return p0 of rule.e_ , rule.f_
  prob_t operator()(const TRule& rule) const {
    return p0(rule.f_, rule.e_, 0, 0);
  }

  prob_t p0(const std::vector<WordID>& vsrc, const std::vector<WordID>& vtrg, int start_src, int start_trg) const;

  const Model1& model1;
  const Model1& invmodel1;
  const prob_t kM1MIXTURE;  // Model 1 mixture component
  const prob_t kUNIFORM_MIXTURE; // uniform mixture component
  const prob_t kUNIFORM_SOURCE;
  const prob_t kUNIFORM_TARGET;
};

// base distribution for jump size multinomials
// basically p(0) = 0 and then, p(1) is max, and then
// you drop as you move to the max jump distance
struct JumpBase {
  JumpBase();

  const prob_t& operator()(int jump, unsigned src_len) const {
    assert(jump != 0);
    const std::map<int, prob_t>::const_iterator it = p[src_len].find(jump);
    assert(it != p[src_len].end());
    return it->second;
  }
  std::vector<std::map<int, prob_t> > p;
};


#endif
