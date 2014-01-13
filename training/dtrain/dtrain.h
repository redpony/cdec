#ifndef _DTRAIN_H_
#define _DTRAIN_H_

#define DTRAIN_DOTS 10 // after how many inputs to display a '.'
#define DTRAIN_SCALE 100000

#include <iomanip>
#include <climits>
#include <string.h>

#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>
#include <boost/algorithm/string/regex.hpp>
#include <boost/program_options.hpp>

#include "decoder.h"
#include "ff_register.h"
#include "sentence_metadata.h"
#include "verbose.h"
#include "viterbi.h"

using namespace std;
namespace po = boost::program_options;

namespace dtrain
{


inline void register_and_convert(const vector<string>& strs, vector<WordID>& ids)
{
  vector<string>::const_iterator it;
  for (it = strs.begin(); it < strs.end(); it++)
    ids.push_back(TD::Convert(*it));
}

inline string gettmpf(const string path, const string infix)
{
  char fn[path.size() + infix.size() + 8];
  strcpy(fn, path.c_str());
  strcat(fn, "/");
  strcat(fn, infix.c_str());
  strcat(fn, "-XXXXXX");
  if (!mkstemp(fn)) {
    cerr << "Cannot make temp file in" << path << " , exiting." << endl;
    exit(1);
  }
  return string(fn);
}

typedef double score_t;

struct ScoredHyp
{
  vector<WordID> w;
  SparseVector<double> f;
  score_t model;
  score_t score;
  unsigned rank;
};

struct LocalScorer
{
  unsigned N_;
  vector<score_t> w_;

  virtual score_t
  Score(const vector<WordID>& hyp, const vector<WordID>& ref, const unsigned rank, const unsigned src_len)=0;

  virtual void Reset() {} // only for ApproxBleuScorer, LinearBleuScorer

  inline void
  Init(unsigned N, vector<score_t> weights)
  {
    assert(N > 0);
    N_ = N;
    if (weights.empty()) for (unsigned i = 0; i < N_; i++) w_.push_back(1./N_);
    else w_ = weights;
  }

  inline score_t
  brevity_penalty(const unsigned hyp_len, const unsigned ref_len)
  {
    if (hyp_len > ref_len) return 1;
    return exp(1 - (score_t)ref_len/hyp_len);
  }
};

struct HypSampler : public DecoderObserver
{
  LocalScorer* scorer_;
  vector<WordID>* ref_;
  unsigned f_count_, sz_;
  virtual vector<ScoredHyp>* GetSamples()=0;
  inline void SetScorer(LocalScorer* scorer) { scorer_ = scorer; }
  inline void SetRef(vector<WordID>& ref) { ref_ = &ref; }
  inline unsigned get_f_count() { return f_count_; }
  inline unsigned get_sz() { return sz_; }
};

struct HSReporter
{
  string task_id_;

  HSReporter(string task_id) : task_id_(task_id) {}

  inline void update_counter(string name, unsigned amount) {
    cerr << "reporter:counter:" << task_id_ << "," << name << "," << amount << endl;
  }
  inline void update_gcounter(string name, unsigned amount) {
    cerr << "reporter:counter:Global," << name << "," << amount << endl;
  }
};

inline ostream& _np(ostream& out) { return out << resetiosflags(ios::showpos); }
inline ostream& _p(ostream& out)  { return out << setiosflags(ios::showpos); }
inline ostream& _p2(ostream& out) { return out << setprecision(2); }
inline ostream& _p5(ostream& out) { return out << setprecision(5); }

inline void printWordIDVec(vector<WordID>& v)
{
  for (unsigned i = 0; i < v.size(); i++) {
    cerr << TD::Convert(v[i]);
    if (i < v.size()-1) cerr << " ";
  }
}

template<typename T>
inline T sign(T z)
{
  if (z == 0) return 0;
  return z < 0 ? -1 : +1;
}


} // namespace

#endif

