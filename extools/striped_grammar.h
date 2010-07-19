#ifndef _STRIPED_GRAMMAR_H_
#define _STRIPED_GRAMMAR_H_

#include <iostream>
#include <boost/functional/hash.hpp>
#include <vector>
#include <tr1/unordered_map>
#include "sparse_vector.h"
#include "wordid.h"
#include "tdict.h"

// represents statistics / information about a rule pair
struct RuleStatistics {
  SparseVector<float> counts;
  std::vector<std::pair<short,short> > aligns;
  RuleStatistics() {}
  RuleStatistics(int name, float val, const std::vector<std::pair<short,short> >& al) :
      aligns(al) {
    counts.set_value(name, val);
  }
  void ParseRuleStatistics(const char* buf, int start, int end);
  RuleStatistics& operator+=(const RuleStatistics& rhs) {
    counts += rhs.counts;
    return *this;
  }
};
std::ostream& operator<<(std::ostream& os, const RuleStatistics& s);

inline void WriteNamed(const std::vector<WordID>& v, std::ostream* os) {
  bool first = true;
  for (int i = 0; i < v.size(); ++i) {
    if (first) { first = false; } else { (*os) << ' '; }
    if (v[i] < 0) { (*os) << '[' << TD::Convert(-v[i]) << ']'; }
    else (*os) << TD::Convert(v[i]);
  }
}

inline void WriteAnonymous(const std::vector<WordID>& v, std::ostream* os) {
  bool first = true;
  for (int i = 0; i < v.size(); ++i) {
    if (first) { first = false; } else { (*os) << ' '; }
    if (v[i] <= 0) { (*os) << '[' << (1-v[i]) << ']'; }
    else (*os) << TD::Convert(v[i]);
  }
}

typedef std::tr1::unordered_map<std::vector<WordID>, RuleStatistics, boost::hash<std::vector<WordID> > > ID2RuleStatistics;

struct StripedGrammarLexer {
  typedef void (*GrammarCallback)(WordID lhs, const std::vector<WordID>& src_rhs, const ID2RuleStatistics& rules, void *extra);
  static void ReadStripedGrammar(std::istream* in, GrammarCallback func, void* extra);
  typedef void (*ContextCallback)(const std::vector<WordID>& phrase, const ID2RuleStatistics& rules, void *extra);
  static void ReadContexts(std::istream* in, ContextCallback func, void* extra);
};

#endif
