#ifndef _TTABLES_H_
#define _TTABLES_H_

#include <iostream>
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
  typedef std::unordered_map<WordID, Word2Double> Word2Word2Double;
  inline double prob(const int& e, const int& f) const {
    const Word2Word2Double::const_iterator cit = ttable.find(e);
    if (cit != ttable.end()) {
      const Word2Double& cpd = cit->second;
      const Word2Double::const_iterator it = cpd.find(f);
      if (it == cpd.end()) return 1e-9;
      return it->second;
    } else {
      return 1e-9;
    }
  }
  inline void Increment(const int& e, const int& f) {
    counts[e][f] += 1.0;
  }
  inline void Increment(const int& e, const int& f, double x) {
    counts[e][f] += x;
  }
  void NormalizeVB(const double alpha) {
    ttable.swap(counts);
    for (Word2Word2Double::iterator cit = ttable.begin();
         cit != ttable.end(); ++cit) {
      double tot = 0;
      Word2Double& cpd = cit->second;
      for (Word2Double::iterator it = cpd.begin(); it != cpd.end(); ++it)
        tot += it->second + alpha;
      for (Word2Double::iterator it = cpd.begin(); it != cpd.end(); ++it)
        it->second = exp(Md::digamma(it->second + alpha) - Md::digamma(tot));
    }
    counts.clear();
  }
  void Normalize() {
    ttable.swap(counts);
    for (Word2Word2Double::iterator cit = ttable.begin();
         cit != ttable.end(); ++cit) {
      double tot = 0;
      Word2Double& cpd = cit->second;
      for (Word2Double::iterator it = cpd.begin(); it != cpd.end(); ++it)
        tot += it->second;
      for (Word2Double::iterator it = cpd.begin(); it != cpd.end(); ++it)
        it->second /= tot;
    }
    counts.clear();
  }
  // adds counts from another TTable - probabilities remain unchanged
  TTable& operator+=(const TTable& rhs) {
    for (Word2Word2Double::const_iterator it = rhs.counts.begin();
         it != rhs.counts.end(); ++it) {
      const Word2Double& cpd = it->second;
      Word2Double& tgt = counts[it->first];
      for (Word2Double::const_iterator j = cpd.begin(); j != cpd.end(); ++j) {
        tgt[j->first] += j->second;
      }
    }
    return *this;
  }
  void ShowTTable() const {
    for (Word2Word2Double::const_iterator it = ttable.begin(); it != ttable.end(); ++it) {
      const Word2Double& cpd = it->second;
      for (Word2Double::const_iterator j = cpd.begin(); j != cpd.end(); ++j) {
        std::cerr << "P(" << TD::Convert(j->first) << '|' << TD::Convert(it->first) << ") = " << j->second << std::endl;
      }
    }
  }
  void ShowCounts() const {
    for (Word2Word2Double::const_iterator it = counts.begin(); it != counts.end(); ++it) {
      const Word2Double& cpd = it->second;
      for (Word2Double::const_iterator j = cpd.begin(); j != cpd.end(); ++j) {
        std::cerr << "c(" << TD::Convert(j->first) << '|' << TD::Convert(it->first) << ") = " << j->second << std::endl;
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
