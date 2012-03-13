#ifndef _UNIGRAMS_H_
#define _UNIGRAMS_H_

#include <vector>
#include <string>
#include <tr1/unordered_map>
#include <boost/functional.hpp>

#include "wordid.h"
#include "prob.h"
#include "tdict.h"

struct UnigramModel {
  explicit UnigramModel(const std::string& fname, unsigned vocab_size) :
      use_uniform_(fname.size() == 0),
      uniform_(1.0 / vocab_size),
      probs_() {
    if (fname.size() > 0) {
      probs_.resize(TD::NumWords() + 1);
      LoadUnigrams(fname);
    }
  }

  const prob_t& operator()(const WordID& w) const {
    assert(w);
    if (use_uniform_) return uniform_;
    return probs_[w];
  }

 private:
  void LoadUnigrams(const std::string& fname);

  const bool use_uniform_;
  const prob_t uniform_;
  std::vector<prob_t> probs_;
};


// reads an ARPA unigram file and converts words like 'cat' into a string 'c a t'
struct UnigramWordModel {
  explicit UnigramWordModel(const std::string& fname) :
      use_uniform_(false),
      uniform_(1.0),
      probs_() {
    LoadUnigrams(fname);
  }

  explicit UnigramWordModel(const unsigned vocab_size) :
      use_uniform_(true),
      uniform_(1.0 / vocab_size),
      probs_() {}

  const prob_t& operator()(const std::vector<WordID>& s) const {
    if (use_uniform_) return uniform_;
    const VectorProbHash::const_iterator it = probs_.find(s);
    assert(it != probs_.end());
    return it->second;
  }

 private:
  void LoadUnigrams(const std::string& fname);

  const bool use_uniform_;
  const prob_t uniform_;
  typedef std::tr1::unordered_map<std::vector<WordID>, prob_t, boost::hash<std::vector<WordID> > > VectorProbHash;
  VectorProbHash probs_;
};

#endif
