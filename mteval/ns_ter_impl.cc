#include "ns_ter_impl.h"

#include <algorithm>
#include <cstdio>
#include <cassert>
#include <iostream>
#include <limits>
#include <unordered_map>
#include <set>
#include <boost/functional/hash.hpp>

static const int ter_short_circuit_long_sentences = -1;

using namespace std;

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


TERScorerImpl::TERScorerImpl(const vector<WordID>& ref) : ref_(ref) {
  for (unsigned i = 0; i < ref.size(); ++i)
    rwexists_.insert(ref[i]);
}

float TERScorerImpl::Calculate(
    const vector<WordID>& hyp,
    int* subs, int* ins, int* dels, int* shifts) const {
  return CalculateAllShifts(hyp, subs, ins, dels, shifts);
}

float TERScorerImpl::MinimumEditDistance(
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
  std::reverse(path->begin(), path->end());
  return cmat[hyp.size()][ref.size()];
}

void TERScorerImpl::BuildWordMatches(
    const vector<WordID>& hyp, NgramToIntsMap* nmap) const {
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

void TERScorerImpl::PerformShift(
    const vector<WordID>& in,
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
    // cerr << "in=" << TD::GetString(in) << endl;
    // cerr << "out=" << TD::GetString(*out) << endl;
  }
  assert(out->size() == in.size());
  // cerr << "ps: " << TD::GetString(*out) << endl;
}

void TERScorerImpl::GetAllPossibleShifts(
    const vector<WordID>& hyp,
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

bool TERScorerImpl::CalculateBestShift(
    const vector<WordID>& cur,
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

void TERScorerImpl::GetPathStats(
    const vector<TransType>& path, int* subs, int* ins, int* dels) {
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

float TERScorerImpl::CalculateAllShifts(
    const vector<WordID>& hyp,
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

} // namespace NewScorer

