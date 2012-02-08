#include "base_distributions.h"

#include <iostream>

#include "filelib.h"

using namespace std;

TableLookupBase::TableLookupBase(const string& fname) {
  cerr << "TableLookupBase reading from " << fname << " ..." << endl;
  ReadFile rf(fname);
  istream& in = *rf.stream();
  string line;
  unsigned lc = 0;
  const WordID kDIV = TD::Convert("|||");
  vector<WordID> tmp;
  vector<int> le, lf;
  TRule x;
  x.lhs_ = -TD::Convert("X");
  bool flag = false;
  while(getline(in, line)) {
    ++lc;
    if (lc % 1000000 == 0) { cerr << " [" << lc << ']' << endl; flag = false; }
    else if (lc % 25000 == 0) { cerr << '.' << flush; flag = true; }
    tmp.clear();
    TD::ConvertSentence(line, &tmp);
    x.f_.clear();
    x.e_.clear();
    size_t pos = 0;
    int cc = 0;
    while(pos < tmp.size()) {
      const WordID cur = tmp[pos++];
      if (cur == kDIV) {
        ++cc;
      } else if (cc == 0) {
        x.f_.push_back(cur);    
      } else if (cc == 1) {
        x.e_.push_back(cur);
      } else if (cc == 2) {
        table[x].logeq(atof(TD::Convert(cur)));
        ++cc;
      } else {
        if (flag) cerr << endl;
        cerr << "Bad format in " << lc << ": " << line << endl; abort();
      }
    }
    if (cc != 3) {
      if (flag) cerr << endl;
      cerr << "Bad format in " << lc << ": " << line << endl; abort();
    }
  }
  if (flag) cerr << endl;
  cerr << " read " << lc << " entries\n";
}

prob_t PhraseConditionalUninformativeUnigramBase::p0(const vector<WordID>& vsrc,
                                                     const vector<WordID>& vtrg,
                                                     int start_src, int start_trg) const {
  const int flen = vsrc.size() - start_src;
  const int elen = vtrg.size() - start_trg;
  prob_t p;
  p.logeq(Md::log_poisson(elen, flen + 0.01));       // elen | flen          ~Pois(flen + 0.01)
  //p.logeq(log_poisson(elen, 1));       // elen | flen          ~Pois(flen + 0.01)
  for (int i = 0; i < elen; ++i)
    p *= u(vtrg[i + start_trg]);                        // draw e_i             ~Uniform
  return p;
}

prob_t PhraseConditionalUninformativeBase::p0(const vector<WordID>& vsrc,
                                              const vector<WordID>& vtrg,
                                              int start_src, int start_trg) const {
  const int flen = vsrc.size() - start_src;
  const int elen = vtrg.size() - start_trg;
  prob_t p;
  //p.logeq(log_poisson(elen, flen + 0.01));       // elen | flen          ~Pois(flen + 0.01)
  p.logeq(Md::log_poisson(elen, 1));       // elen | flen          ~Pois(flen + 0.01)
  for (int i = 0; i < elen; ++i)
    p *= kUNIFORM_TARGET;                        // draw e_i             ~Uniform
  return p;
}

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
  p.logeq(Md::log_poisson(elen, flen + 0.01));       // elen | flen          ~Pois(flen + 0.01)
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
  p.logeq(Md::log_poisson(flen, 1.0));               // flen                 ~Pois(1)
                                                 // elen | flen          ~Pois(flen + 0.01)
  prob_t ptrglen; ptrglen.logeq(Md::log_poisson(elen, flen + 0.01));
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

prob_t PhraseJointBase_BiDir::p0(const vector<WordID>& vsrc,
                                 const vector<WordID>& vtrg,
                                 int start_src, int start_trg) const {
  const int flen = vsrc.size() - start_src;
  const int elen = vtrg.size() - start_trg;
  prob_t uniform_src_alignment; uniform_src_alignment.logeq(-log(flen + 1));
  prob_t uniform_trg_alignment; uniform_trg_alignment.logeq(-log(elen + 1));

  prob_t p1;
  p1.logeq(Md::log_poisson(flen, 1.0));               // flen                 ~Pois(1)
                                                 // elen | flen          ~Pois(flen + 0.01)
  prob_t ptrglen; ptrglen.logeq(Md::log_poisson(elen, flen + 0.01));
  p1 *= ptrglen;
  p1 *= kUNIFORM_SOURCE.pow(flen);                // each f in F ~Uniform
  for (int i = 0; i < elen; ++i) {               // for each position i in E
    const WordID trg = vtrg[i + start_trg];
    prob_t tp = prob_t::Zero();
    for (int j = -1; j < flen; ++j) {
      const WordID src = j < 0 ? 0 : vsrc[j + start_src];
      tp += kM1MIXTURE * model1(src, trg);
      tp += kUNIFORM_MIXTURE * kUNIFORM_TARGET;
    }
    tp *= uniform_src_alignment;                 //     draw a_i         ~uniform
    p1 *= tp;                                     //     draw e_i         ~Model1(f_a_i) / uniform
  }
  if (p1.is_0()) {
    cerr << "Zero! " << vsrc << "\nTRG=" << vtrg << endl;
    abort();
  }

  prob_t p2;
  p2.logeq(Md::log_poisson(elen, 1.0));               // elen                 ~Pois(1)
                                                 // flen | elen          ~Pois(flen + 0.01)
  prob_t psrclen; psrclen.logeq(Md::log_poisson(flen, elen + 0.01));
  p2 *= psrclen;
  p2 *= kUNIFORM_TARGET.pow(elen);                // each f in F ~Uniform
  for (int i = 0; i < flen; ++i) {               // for each position i in E
    const WordID src = vsrc[i + start_src];
    prob_t tp = prob_t::Zero();
    for (int j = -1; j < elen; ++j) {
      const WordID trg = j < 0 ? 0 : vtrg[j + start_trg];
      tp += kM1MIXTURE * invmodel1(trg, src);
      tp += kUNIFORM_MIXTURE * kUNIFORM_SOURCE;
    }
    tp *= uniform_trg_alignment;                 //     draw a_i         ~uniform
    p2 *= tp;                                     //     draw e_i         ~Model1(f_a_i) / uniform
  }
  if (p2.is_0()) {
    cerr << "Zero! " << vsrc << "\nTRG=" << vtrg << endl;
    abort();
  }

  static const prob_t kHALF(0.5);
  return (p1 + p2) * kHALF;
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
        cp.logeq(Md::log_poisson(1.5-j, 1));
      else if (j > 0)
        cp.logeq(Md::log_poisson(j, 1));
      cp.poweq(0.2);
      z += cp;
    }
    for (int j = min_jump; j <= max_jump; ++j) {
      cpd[j] /= z;
    }
  }
}

