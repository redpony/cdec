#include "ns_ter.h"

#include <cstdio>
#include <cassert>
#include <iostream>
#include <limits>
#ifndef HAVE_OLD_CPP
# include <unordered_map>
#else
# include <tr1/unordered_map>
namespace std { using std::tr1::unordered_map; }
#endif
#include <set>
#include <boost/functional/hash.hpp>
#include "ns_ter_impl.h"
#include "tdict.h"

static const bool ter_use_average_ref_len = true;

static const unsigned kINSERTIONS = 0;
static const unsigned kDELETIONS = 1;
static const unsigned kSUBSTITUTIONS = 2;
static const unsigned kSHIFTS = 3;
static const unsigned kREF_WORDCOUNT = 4;
static const unsigned kDUMMY_LAST_ENTRY = 5;

using namespace std;

bool TERMetric::IsErrorMetric() const {
  return true;
}

void TERMetric::ComputeSufficientStatistics(const vector<WordID>& hyp,
                                            const vector<vector<WordID> >& refs,
                                            SufficientStats* out) const {
  out->fields.resize(kDUMMY_LAST_ENTRY);
  float best_score = numeric_limits<float>::max();
  unsigned avg_len = 0;
  for (int i = 0; i < refs.size(); ++i)
    avg_len += refs[i].size();
  avg_len /= refs.size();

  for (int i = 0; i < refs.size(); ++i) {
    int subs, ins, dels, shifts;
    NewScorer::TERScorerImpl ter(refs[i]);
    float score = ter.Calculate(hyp, &subs, &ins, &dels, &shifts);
    // cerr << "Component TER cost: " << score << endl;
    if (score < best_score) {
      out->fields[kINSERTIONS] = ins;
      out->fields[kDELETIONS] = dels;
      out->fields[kSUBSTITUTIONS] = subs;
      out->fields[kSHIFTS] = shifts;
      if (ter_use_average_ref_len) {
        out->fields[kREF_WORDCOUNT] = avg_len;
      } else {
        out->fields[kREF_WORDCOUNT] = refs[i].size();
      }

      best_score = score;
    }
  }
}

unsigned TERMetric::SufficientStatisticsVectorSize() const {
  return kDUMMY_LAST_ENTRY;
}

float TERMetric::ComputeScore(const SufficientStats& stats) const {
  float edits = static_cast<float>(stats[kINSERTIONS] + stats[kDELETIONS] + stats[kSUBSTITUTIONS] + stats[kSHIFTS]);
  return edits / static_cast<float>(stats[kREF_WORDCOUNT]);
}

string TERMetric::DetailedScore(const SufficientStats& stats) const {
  char buf[200];
  sprintf(buf, "TER = %.2f, %3.f|%3.f|%3.f|%3.f (len=%3.f)",
     ComputeScore(stats) * 100.0f,
     stats[kINSERTIONS],
     stats[kDELETIONS],
     stats[kSUBSTITUTIONS],
     stats[kSHIFTS],
     stats[kREF_WORDCOUNT]);
  return buf;
}

