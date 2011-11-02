#ifndef _BASE_MEASURES_H_
#define _BASE_MEASURES_H_

#include <vector>
#include <map>
#include <string>
#include <cmath>
#include <iostream>

#include "trule.h"
#include "prob.h"
#include "tdict.h"

inline double log_poisson(unsigned x, const double& lambda) {
  assert(lambda > 0.0);
  return log(lambda) * x - lgamma(x + 1) - lambda;
}

inline std::ostream& operator<<(std::ostream& os, const std::vector<WordID>& p) {
  os << '[';
  for (int i = 0; i < p.size(); ++i)
    os << (i==0 ? "" : " ") << TD::Convert(p[i]);
  return os << ']';
}

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

  // return p0 of rule.e_ | rule.f_
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

  // return p0 of rule.e_ | rule.f_
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
