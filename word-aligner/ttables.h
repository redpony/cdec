#ifndef _TTABLES_H_
#define _TTABLES_H_

#include <iostream>
#include <vector>
#ifndef HAVE_OLD_CPP
# include <unordered_map>
#else
# include <tr1/unordered_map>
namespace std { using std::tr1::unordered_map; }
#endif

#include "sparse_vector.h"
#include "m.h"
#include "wordid.h"
#include "tdict.h"

class TTable {
 public:
  TTable() {}
  typedef std::unordered_map<WordID, double> Word2Double;
  typedef std::vector<Word2Double> Word2Word2Double;
  inline double prob(const int& e, const int& f) const {
    if (e < static_cast<int>(ttable.size())) {
      const Word2Double& cpd = ttable[e];
      const Word2Double::const_iterator it = cpd.find(f);
      if (it == cpd.end()) return 1e-9;
      return it->second;
    } else {
      return 1e-9;
    }
  }
  inline void Increment(const int& e, const int& f) {
    if (e >= static_cast<int>(ttable.size())) counts.resize(e + 1);
    counts[e][f] += 1.0;
  }
  inline void Increment(const int& e, const int& f, double x) {
    if (e >= static_cast<int>(counts.size())) counts.resize(e + 1);
    counts[e][f] += x;
  }
  void NormalizeVB(const double alpha) {
    ttable.swap(counts);
    for (unsigned i = 0; i < ttable.size(); ++i) {
      double tot = 0;
      Word2Double& cpd = ttable[i];
      for (Word2Double::iterator it = cpd.begin(); it != cpd.end(); ++it)
        tot += it->second + alpha;
      if (!tot) tot = 1;
      for (Word2Double::iterator it = cpd.begin(); it != cpd.end(); ++it)
        it->second = exp(Md::digamma(it->second + alpha) - Md::digamma(tot));
    }
    counts.clear();
  }
  void Normalize() {
    ttable.swap(counts);
    for (unsigned i = 0; i < ttable.size(); ++i) {
      double tot = 0;
      Word2Double& cpd = ttable[i];
      for (Word2Double::iterator it = cpd.begin(); it != cpd.end(); ++it)
        tot += it->second;
      if (!tot) tot = 1;
      for (Word2Double::iterator it = cpd.begin(); it != cpd.end(); ++it)
        it->second /= tot;
    }
    counts.clear();
  }
  // adds counts from another TTable - probabilities remain unchanged
  TTable& operator+=(const TTable& rhs) {
    if (rhs.counts.size() > counts.size()) counts.resize(rhs.counts.size());
    for (unsigned i = 0; i < rhs.counts.size(); ++i) {
      const Word2Double& cpd = rhs.counts[i];
      Word2Double& tgt = counts[i];
      for (auto p : cpd) tgt[p.first] += p.second;
    }
    return *this;
  }
  void ShowTTable() const {
    for (unsigned it = 0; it < ttable.size(); ++it) {
      const Word2Double& cpd = ttable[it];
      for (auto& p : cpd) {
        std::cerr << "c(" << TD::Convert(p.first) << '|' << TD::Convert(it) << ") = " << p.second << std::endl;
      }
    }
  }
  void ShowCounts() const {
    for (unsigned it = 0; it < counts.size(); ++it) {
      const Word2Double& cpd = counts[it];
      for (auto& p : cpd) {
        std::cerr << "c(" << TD::Convert(p.first) << '|' << TD::Convert(it) << ") = " << p.second << std::endl;
      }
    }
  }
  void DeserializeProbsFromText(std::istream* in);
  void DeserializeLogProbsFromText(std::istream* in);
  void SerializeCounts(std::string* out) const { SerializeHelper(out, counts); }
  void DeserializeCounts(const std::string& in) { DeserializeHelper(in, &counts); }
  void SerializeProbs(std::string* out) const { SerializeHelper(out, ttable); }
  void DeserializeProbs(const std::string& in) { DeserializeHelper(in, &ttable); }
 private:
  static void SerializeHelper(std::string*, const Word2Word2Double& o);
  static void DeserializeHelper(const std::string&, Word2Word2Double* o);
 public:
  Word2Word2Double ttable;
  Word2Word2Double counts;
};

#endif
