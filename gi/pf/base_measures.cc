#include "base_measures.h"

#include <iostream>

#include "filelib.h"

using namespace std;

void Model1::LoadModel1(const string& fname) {
  cerr << "Loading Model 1 parameters from " << fname << " ..." << endl;
  ReadFile rf(fname);
  istream& in = *rf.stream();
  string line;
  unsigned lc = 0;
  while(getline(in, line)) {
    ++lc;
    int cur = 0;
    int start = 0;
    while(cur < line.size() && line[cur] != ' ') { ++cur; }
    assert(cur != line.size());
    line[cur] = 0;
    const WordID src = TD::Convert(&line[0]);
    ++cur;
    start = cur;
    while(cur < line.size() && line[cur] != ' ') { ++cur; }
    assert(cur != line.size());
    line[cur] = 0;
    WordID trg = TD::Convert(&line[start]);
    const double logprob = strtod(&line[cur + 1], NULL);
    if (src >= ttable.size()) ttable.resize(src + 1);
    ttable[src][trg].logeq(logprob);
  }
  cerr << "  read " << lc << " parameters.\n";
}

prob_t PhraseConditionalBase::p0(const vector<WordID>& vsrc,
                                 const vector<WordID>& vtrg,
                                 int start_src, int start_trg) const {
  const int flen = vsrc.size() - start_src;
  const int elen = vtrg.size() - start_trg;
  prob_t uniform_src_alignment; uniform_src_alignment.logeq(-log(flen + 1));
  prob_t p;
  p.logeq(log_poisson(elen, flen + 0.01));       // elen | flen          ~Pois(flen + 0.01)
  for (int i = 0; i < elen; ++i) {               // for each position i in e-RHS
    const WordID trg = vtrg[i + start_trg];
    prob_t tp = prob_t::Zero();
    for (int j = -1; j < flen; ++j) {
      const WordID src = j < 0 ? 0 : vsrc[j + start_src];
      tp += kM1MIXTURE * model1(src, trg);
      tp += kUNIFORM_MIXTURE * kUNIFORM_TARGET;
    }
    tp *= uniform_src_alignment;                 //     draw a_i         ~uniform
    p *= tp;                                     //     draw e_i         ~Model1(f_a_i) / uniform
  }
  if (p.is_0()) {
    cerr << "Zero! " << vsrc << "\nTRG=" << vtrg << endl;
    abort();
  }
  return p;
}

prob_t PhraseJointBase::p0(const vector<WordID>& vsrc,
                           const vector<WordID>& vtrg,
                           int start_src, int start_trg) const {
  const int flen = vsrc.size() - start_src;
  const int elen = vtrg.size() - start_trg;
  prob_t uniform_src_alignment; uniform_src_alignment.logeq(-log(flen + 1));
  prob_t p;
  p.logeq(log_poisson(flen, 1.0));               // flen                 ~Pois(1)
                                                 // elen | flen          ~Pois(flen + 0.01)
  prob_t ptrglen; ptrglen.logeq(log_poisson(elen, flen + 0.01));
  p *= ptrglen;
  p *= kUNIFORM_SOURCE.pow(flen);                // each f in F ~Uniform
  for (int i = 0; i < elen; ++i) {               // for each position i in E
    const WordID trg = vtrg[i + start_trg];
    prob_t tp = prob_t::Zero();
    for (int j = -1; j < flen; ++j) {
      const WordID src = j < 0 ? 0 : vsrc[j + start_src];
      tp += kM1MIXTURE * model1(src, trg);
      tp += kUNIFORM_MIXTURE * kUNIFORM_TARGET;
    }
    tp *= uniform_src_alignment;                 //     draw a_i         ~uniform
    p *= tp;                                     //     draw e_i         ~Model1(f_a_i) / uniform
  }
  if (p.is_0()) {
    cerr << "Zero! " << vsrc << "\nTRG=" << vtrg << endl;
    abort();
  }
  return p;
}

JumpBase::JumpBase() : p(200) {
  for (unsigned src_len = 1; src_len < 200; ++src_len) {
    map<int, prob_t>& cpd = p[src_len];
    int min_jump = 1 - src_len;
    int max_jump = src_len;
    prob_t z;
    for (int j = min_jump; j <= max_jump; ++j) {
      prob_t& cp = cpd[j];
      if (j < 0)
        cp.logeq(log_poisson(1.5-j, 1));
      else if (j > 0)
        cp.logeq(log_poisson(j, 1));
      cp.poweq(0.2);
      z += cp;
    }
    for (int j = min_jump; j <= max_jump; ++j) {
      cpd[j] /= z;
    }
  }
}

