#include "candidate_set.h"

#ifndef HAVE_OLD_CPP
# include <unordered_set>
#else
# include <tr1/unordered_set>
namespace std { using std::tr1::unordered_set; }
#endif

#include <boost/functional/hash.hpp>

#include "verbose.h"
#include "ns.h"
#include "filelib.h"
#include "wordid.h"
#include "tdict.h"
#include "hg.h"
#include "kbest.h"
#include "viterbi.h"

using namespace std;

namespace training {

struct ApproxVectorHasher {
  static const size_t MASK = 0xFFFFFFFFull;
  union UType {
    double f;   // leave as double
    size_t i;
  };
  static inline double round(const double x) {
    UType t;
    t.f = x;
    size_t r = t.i & MASK;
    if ((r << 1) > MASK)
      t.i += MASK - r + 1;
    else
      t.i &= (1ull - MASK);
    return t.f;
  }
  size_t operator()(const SparseVector<double>& x) const {
    size_t h = 0x573915839;
    for (SparseVector<double>::const_iterator it = x.begin(); it != x.end(); ++it) {
      UType t;
      t.f = it->second;
      if (t.f) {
        size_t z = (t.i >> 32);
        boost::hash_combine(h, it->first);
        boost::hash_combine(h, z);
      }
    }
    return h;
  }
};

struct ApproxVectorEquals {
  bool operator()(const SparseVector<double>& a, const SparseVector<double>& b) const {
    SparseVector<double>::const_iterator bit = b.begin();
    for (SparseVector<double>::const_iterator ait = a.begin(); ait != a.end(); ++ait) {
      if (bit == b.end() ||
          ait->first != bit->first ||
          ApproxVectorHasher::round(ait->second) != ApproxVectorHasher::round(bit->second))
        return false;
      ++bit;
    }
    if (bit != b.end()) return false;
    return true;
  }
};

struct CandidateCompare {
  bool operator()(const Candidate& a, const Candidate& b) const {
    ApproxVectorEquals eq;
    return (a.ewords == b.ewords && eq(a.fmap,b.fmap));
  }
};

struct CandidateHasher {
  size_t operator()(const Candidate& x) const {
    boost::hash<vector<WordID> > hhasher;
    ApproxVectorHasher vhasher;
    size_t ha = hhasher(x.ewords);
    boost::hash_combine(ha, vhasher(x.fmap));
    return ha;
  }
};

static void ParseSparseVector(string& line, size_t cur, SparseVector<double>* out) {
  SparseVector<double>& x = *out;
  size_t last_start = cur;
  size_t last_comma = string::npos;
  while(cur <= line.size()) {
    if (line[cur] == ' ' || cur == line.size()) {
      if (!(cur > last_start && last_comma != string::npos && cur > last_comma)) {
        cerr << "[ERROR] " << line << endl << "  position = " << cur << endl;
        exit(1);
      }
      const int fid = FD::Convert(line.substr(last_start, last_comma - last_start));
      if (cur < line.size()) line[cur] = 0;
      const double val = strtod(&line[last_comma + 1], NULL);
      x.set_value(fid, val);

      last_comma = string::npos;
      last_start = cur+1;
    } else {
      if (line[cur] == '=')
        last_comma = cur;
    }
    ++cur;
  }
}

void CandidateSet::WriteToFile(const string& file) const {
  WriteFile wf(file);
  ostream& out = *wf.stream();
  out.precision(10);
  string ss;
  for (unsigned i = 0; i < cs.size(); ++i) {
    out << TD::GetString(cs[i].ewords) << endl;
    out << cs[i].fmap << endl;
    cs[i].eval_feats.Encode(&ss);
    out << ss << endl;
  }
}

void CandidateSet::ReadFromFile(const string& file) {
  if(!SILENT) cerr << "Reading candidates from " << file << endl;
  ReadFile rf(file);
  istream& in = *rf.stream();
  string cand;
  string feats;
  string ss;
  while(getline(in, cand)) {
    getline(in, feats);
    getline(in, ss);
    assert(in);
    cs.push_back(Candidate());
    TD::ConvertSentence(cand, &cs.back().ewords);
    ParseSparseVector(feats, 0, &cs.back().fmap);
    cs.back().eval_feats = SufficientStats(ss);
  }
  if(!SILENT) cerr << "  read " << cs.size() << " candidates\n";
}

void CandidateSet::Dedup() {
  if(!SILENT) cerr << "Dedup in=" << cs.size();
  unordered_set<Candidate, CandidateHasher, CandidateCompare> u;
  while(cs.size() > 0) {
    u.insert(cs.back());
    cs.pop_back();
  }
  unordered_set<Candidate, CandidateHasher, CandidateCompare>::iterator it = u.begin();
  while (it != u.end()) {
    cs.push_back(*it);
    it = u.erase(it);
  }
  if(!SILENT) cerr << "  out=" << cs.size() << endl;
}

void CandidateSet::AddKBestCandidates(const Hypergraph& hg, size_t kbest_size, const SegmentEvaluator* scorer) {
  KBest::KBestDerivations<vector<WordID>, ESentenceTraversal> kbest(hg, kbest_size);

  for (unsigned i = 0; i < kbest_size; ++i) {
    const KBest::KBestDerivations<vector<WordID>, ESentenceTraversal>::Derivation* d =
      kbest.LazyKthBest(hg.nodes_.size() - 1, i);
    if (!d) break;
    cs.push_back(Candidate(d->yield, d->feature_values));
    if (scorer)
      scorer->Evaluate(d->yield, &cs.back().eval_feats);
  }
  Dedup();
}

}
