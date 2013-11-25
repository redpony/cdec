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
#include "tdict.h"

static const bool ter_use_average_ref_len = true;
static const int ter_short_circuit_long_sentences = -1;

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

namespace NewScorer {

struct COSTS {
  static const float substitution;
  static const float deletion;
  static const float insertion;
  static const float shift;
};
const float COSTS::substitution = 1.0f;
const float COSTS::deletion = 1.0f;
const float COSTS::insertion = 1.0f;
const float COSTS::shift = 1.0f;

static const int MAX_SHIFT_SIZE = 10;
static const int MAX_SHIFT_DIST = 50;

struct Shift {
  unsigned int d_;
  Shift() : d_() {}
  Shift(int b, int e, int m) : d_() {
    begin(b);
    end(e);
    moveto(m);
  }
  inline int begin() const {
    return d_ & 0x3ff;
  }
  inline int end() const {
    return (d_ >> 10) & 0x3ff;
  }
  inline int moveto() const {
    int m = (d_ >> 20) & 0x7ff;
    if (m > 1024) { m -= 1024; m *= -1; }
    return m;
  }
  inline void begin(int b) {
    d_ &= 0xfffffc00u;
    d_ |= (b & 0x3ff);
  }
  inline void end(int e) {
    d_ &= 0xfff003ffu;
    d_ |= (e & 0x3ff) << 10;
  }
  inline void moveto(int m) {
    bool neg = (m < 0);
    if (neg) { m *= -1; m += 1024; }
    d_ &= 0xfffff;
    d_ |= (m & 0x7ff) << 20;
  }
};

class TERScorerImpl {

 public:
  enum TransType { MATCH, SUBSTITUTION, INSERTION, DELETION };

  explicit TERScorerImpl(const vector<WordID>& ref) : ref_(ref) {
    for (unsigned i = 0; i < ref.size(); ++i)
      rwexists_.insert(ref[i]);
  }

  float Calculate(const vector<WordID>& hyp, int* subs, int* ins, int* dels, int* shifts) const {
    return CalculateAllShifts(hyp, subs, ins, dels, shifts);
  }

  inline int GetRefLength() const {
    return ref_.size();
  }

 private:
  const vector<WordID>& ref_;
  set<WordID> rwexists_;

  typedef unordered_map<vector<WordID>, set<int>, boost::hash<vector<WordID> > > NgramToIntsMap;
  mutable NgramToIntsMap nmap_;

  static float MinimumEditDistance(
      const vector<WordID>& hyp,
      const vector<WordID>& ref,
      vector<TransType>* path) {
    vector<vector<TransType> > bmat(hyp.size() + 1, vector<TransType>(ref.size() + 1, MATCH));
    vector<vector<float> > cmat(hyp.size() + 1, vector<float>(ref.size() + 1, 0));
    for (int i = 0; i <= hyp.size(); ++i)
      cmat[i][0] = i;
    for (int j = 0; j <= ref.size(); ++j)
      cmat[0][j] = j;
    for (int i = 1; i <= hyp.size(); ++i) {
      const WordID& hw = hyp[i-1];
      for (int j = 1; j <= ref.size(); ++j) {
        const WordID& rw = ref[j-1];
	float& cur_c = cmat[i][j];
	TransType& cur_b = bmat[i][j];

        if (rw == hw) {
          cur_c = cmat[i-1][j-1];
          cur_b = MATCH;
        } else {
          cur_c = cmat[i-1][j-1] + COSTS::substitution;
          cur_b = SUBSTITUTION;
        }
	float cwoi = cmat[i-1][j];
        if (cur_c > cwoi + COSTS::insertion) {
          cur_c = cwoi + COSTS::insertion;
          cur_b = INSERTION;
        }
        float cwod = cmat[i][j-1];
        if (cur_c > cwod + COSTS::deletion) {
          cur_c = cwod + COSTS::deletion;
          cur_b = DELETION;
        }
      }
    }

    // trace back along the best path and record the transition types
    path->clear();
    int i = hyp.size();
    int j = ref.size();
    while (i > 0 || j > 0) {
      if (j == 0) {
        --i;
        path->push_back(INSERTION);
      } else if (i == 0) {
        --j;
        path->push_back(DELETION);
      } else {
        TransType t = bmat[i][j];
        path->push_back(t);
        switch (t) {
          case SUBSTITUTION:
          case MATCH:
            --i; --j; break;
          case INSERTION:
            --i; break;
          case DELETION:
            --j; break;
        }
      }
    }
    reverse(path->begin(), path->end());
    return cmat[hyp.size()][ref.size()];
  }

  void BuildWordMatches(const vector<WordID>& hyp, NgramToIntsMap* nmap) const {
    nmap->clear();
    set<WordID> exists_both;
    for (int i = 0; i < hyp.size(); ++i)
      if (rwexists_.find(hyp[i]) != rwexists_.end())
        exists_both.insert(hyp[i]);
    for (int start=0; start<ref_.size(); ++start) {
      if (exists_both.find(ref_[start]) == exists_both.end()) continue;
      vector<WordID> cp;
      int mlen = min(MAX_SHIFT_SIZE, static_cast<int>(ref_.size() - start));
      for (int len=0; len<mlen; ++len) {
        if (len && exists_both.find(ref_[start + len]) == exists_both.end()) break;
        cp.push_back(ref_[start + len]);
	(*nmap)[cp].insert(start);
      }
    }
  }

  static void PerformShift(const vector<WordID>& in,
    int start, int end, int moveto, vector<WordID>* out) {
    // cerr << "ps: " << start << " " << end << " " << moveto << endl;
    out->clear();
    if (moveto == -1) {
      for (int i = start; i <= end; ++i)
       out->push_back(in[i]);
      for (int i = 0; i < start; ++i)
       out->push_back(in[i]);
      for (int i = end+1; i < in.size(); ++i)
       out->push_back(in[i]);
    } else if (moveto < start) {
      for (int i = 0; i <= moveto; ++i)
       out->push_back(in[i]);
      for (int i = start; i <= end; ++i)
       out->push_back(in[i]);
      for (int i = moveto+1; i < start; ++i)
       out->push_back(in[i]);
      for (int i = end+1; i < in.size(); ++i)
       out->push_back(in[i]);
    } else if (moveto > end) {
      for (int i = 0; i < start; ++i)
       out->push_back(in[i]);
      for (int i = end+1; i <= moveto; ++i)
       out->push_back(in[i]);
      for (int i = start; i <= end; ++i)
       out->push_back(in[i]);
      for (int i = moveto+1; i < in.size(); ++i)
       out->push_back(in[i]);
    } else {
      for (int i = 0; i < start; ++i)
       out->push_back(in[i]);
      for (int i = end+1; (i < in.size()) && (i <= end + (moveto - start)); ++i)
       out->push_back(in[i]);
      for (int i = start; i <= end; ++i)
       out->push_back(in[i]);
      for (int i = (end + (moveto - start))+1; i < in.size(); ++i)
       out->push_back(in[i]);
    }
    if (out->size() != in.size()) {
      cerr << "ps: " << start << " " << end << " " << moveto << endl;
      cerr << "in=" << TD::GetString(in) << endl;
      cerr << "out=" << TD::GetString(*out) << endl;
    }
    assert(out->size() == in.size());
    // cerr << "ps: " << TD::GetString(*out) << endl;
  }

  void GetAllPossibleShifts(const vector<WordID>& hyp,
      const vector<int>& ralign,
      const vector<bool>& herr,
      const vector<bool>& rerr,
      const int min_size,
      vector<vector<Shift> >* shifts) const {
    for (int start = 0; start < hyp.size(); ++start) {
      vector<WordID> cp(1, hyp[start]);
      NgramToIntsMap::iterator niter = nmap_.find(cp);
      if (niter == nmap_.end()) continue;
      bool ok = false;
      int moveto;
      for (set<int>::iterator i = niter->second.begin(); i != niter->second.end(); ++i) {
        moveto = *i;
        int rm = ralign[moveto];
        ok = (start != rm &&
              (rm - start) < MAX_SHIFT_DIST &&
              (start - rm - 1) < MAX_SHIFT_DIST);
        if (ok) break;
      }
      if (!ok) continue;
      cp.clear();
      for (int end = start + min_size - 1;
           ok && end < hyp.size() && end < (start + MAX_SHIFT_SIZE); ++end) {
        cp.push_back(hyp[end]);
	vector<Shift>& sshifts = (*shifts)[end - start];
        ok = false;
        NgramToIntsMap::iterator niter = nmap_.find(cp);
        if (niter == nmap_.end()) break;
        bool any_herr = false;
        for (int i = start; i <= end && !any_herr; ++i)
          any_herr = herr[i];
        if (!any_herr) {
          ok = true;
          continue;
        }
        for (set<int>::iterator mi = niter->second.begin();
             mi != niter->second.end(); ++mi) {
          int moveto = *mi;
	  int rm = ralign[moveto];
	  if (! ((rm != start) &&
	        ((rm < start) || (rm > end)) &&
		(rm - start <= MAX_SHIFT_DIST) &&
		((start - rm - 1) <= MAX_SHIFT_DIST))) continue;
          ok = true;
	  bool any_rerr = false;
	  for (int i = 0; (i <= end - start) && (!any_rerr); ++i)
            any_rerr = rerr[moveto+i];
	  if (!any_rerr) continue;
	  for (int roff = 0; roff <= (end - start); ++roff) {
	    int rmr = ralign[moveto+roff];
	    if ((start != rmr) && ((roff == 0) || (rmr != ralign[moveto])))
	      sshifts.push_back(Shift(start, end, moveto + roff));
	  }
        }
      }
    }
  }

  bool CalculateBestShift(const vector<WordID>& cur,
                          const vector<WordID>& /*hyp*/,
                          float curerr,
                          const vector<TransType>& path,
                          vector<WordID>* new_hyp,
                          float* newerr,
                          vector<TransType>* new_path) const {
    vector<bool> herr, rerr;
    vector<int> ralign;
    int hpos = -1;
    for (int i = 0; i < path.size(); ++i) {
      switch (path[i]) {
        case MATCH:
	  ++hpos;
	  herr.push_back(false);
	  rerr.push_back(false);
	  ralign.push_back(hpos);
          break;
        case SUBSTITUTION:
	  ++hpos;
	  herr.push_back(true);
	  rerr.push_back(true);
	  ralign.push_back(hpos);
          break;
        case INSERTION:
	  ++hpos;
	  herr.push_back(true);
          break;
	case DELETION:
	  rerr.push_back(true);
	  ralign.push_back(hpos);
          break;
      }
    }
#if 0
    cerr << "RALIGN: ";
    for (int i = 0; i < rerr.size(); ++i)
      cerr << ralign[i] << " ";
    cerr << endl;
    cerr << "RERR: ";
    for (int i = 0; i < rerr.size(); ++i)
      cerr << (bool)rerr[i] << " ";
    cerr << endl;
    cerr << "HERR: ";
    for (int i = 0; i < herr.size(); ++i)
      cerr << (bool)herr[i] << " ";
    cerr << endl;
#endif

    vector<vector<Shift> > shifts(MAX_SHIFT_SIZE + 1);
    GetAllPossibleShifts(cur, ralign, herr, rerr, 1, &shifts);
    float cur_best_shift_cost = 0;
    *newerr = curerr;
    vector<TransType> cur_best_path;
    vector<WordID> cur_best_hyp;

    bool res = false;
    for (int i = shifts.size() - 1; i >=0; --i) {
      float curfix = curerr - (cur_best_shift_cost + *newerr);
      float maxfix = 2.0f * (1 + i) - COSTS::shift;
      if ((curfix > maxfix) || ((cur_best_shift_cost == 0) && (curfix == maxfix))) break;
      for (int j = 0; j < shifts[i].size(); ++j) {
        const Shift& s = shifts[i][j];
	curfix = curerr - (cur_best_shift_cost + *newerr);
	maxfix = 2.0f * (1 + i) - COSTS::shift;  // TODO remove?
        if ((curfix > maxfix) || ((cur_best_shift_cost == 0) && (curfix == maxfix))) continue;
	vector<WordID> shifted(cur.size());
	PerformShift(cur, s.begin(), s.end(), ralign[s.moveto()], &shifted);
	vector<TransType> try_path;
	float try_cost = MinimumEditDistance(shifted, ref_, &try_path);
	float gain = (*newerr + cur_best_shift_cost) - (try_cost + COSTS::shift);
	if (gain > 0.0f || ((cur_best_shift_cost == 0.0f) && (gain == 0.0f))) {
	  *newerr = try_cost;
	  cur_best_shift_cost = COSTS::shift;
	  new_path->swap(try_path);
	  new_hyp->swap(shifted);
	  res = true;
	  // cerr << "Found better shift " << s.begin() << "..." << s.end() << " moveto " << s.moveto() << endl;
	}
      }
    }

    return res;
  }

  static void GetPathStats(const vector<TransType>& path, int* subs, int* ins, int* dels) {
    *subs = *ins = *dels = 0;
    for (int i = 0; i < path.size(); ++i) {
      switch (path[i]) {
        case SUBSTITUTION:
	  ++(*subs);
        case MATCH:
          break;
        case INSERTION:
          ++(*ins); break;
	case DELETION:
          ++(*dels); break;
      }
    }
  }

  float CalculateAllShifts(const vector<WordID>& hyp,
      int* subs, int* ins, int* dels, int* shifts) const {
    BuildWordMatches(hyp, &nmap_);
    vector<TransType> path;
    float med_cost = MinimumEditDistance(hyp, ref_, &path);
    float edits = 0;
    vector<WordID> cur = hyp;
    *shifts = 0;
    if (ter_short_circuit_long_sentences < 0 ||
        ref_.size() < ter_short_circuit_long_sentences) {
      while (true) {
        vector<WordID> new_hyp;
        vector<TransType> new_path;
        float new_med_cost;
        if (!CalculateBestShift(cur, hyp, med_cost, path, &new_hyp, &new_med_cost, &new_path))
          break;
        edits += COSTS::shift;
        ++(*shifts);
        med_cost = new_med_cost;
        path.swap(new_path);
        cur.swap(new_hyp);
      }
    }
    GetPathStats(path, subs, ins, dels);
    return med_cost + edits;
  }
};

#if 0
void TERScore::ScoreDetails(std::string* details) const {
  char buf[200];
  sprintf(buf, "TER = %.2f, %3d|%3d|%3d|%3d (len=%d)",
     ComputeScore() * 100.0f,
     stats[kINSERTIONS],
     stats[kDELETIONS],
     stats[kSUBSTITUTIONS],
     stats[kSHIFTS],
     stats[kREF_WORDCOUNT]);
  *details = buf;
}
#endif

} // namespace NewScorer

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

