#include <iostream>
#include <string>
#include <queue>
#include <sstream>

#include "alignment_io.h"
#include "tdict.h"

using namespace std;

enum CombinationType {
  kNONE = 0,
  kAXIOM,
  kMONO, kSWAP, kCONTAINS_L, kCONTAINS_R, kINTERLEAVE
};

string nm(CombinationType x) {
  switch (x) {
    case kNONE: return "NONE";
    case kAXIOM: return "AXIOM";
    case kMONO: return "MONO";
    case kSWAP: return "SWAP";
    case kCONTAINS_L: return "CONTAINS_L";
    case kCONTAINS_R: return "CONTAINS_R";
    case kINTERLEAVE: return "INTERLEAVE";
  }
}

string Substring(const vector<WordID>& s, unsigned i, unsigned j) {
  ostringstream os;
  for (unsigned k = i; k < j; ++k) {
    if (k > i) os << ' ';
    os << TD::Convert(s[k]);
  }
  return os.str();
}

inline int min4(int a, int b, int c, int d) {
  int l = a;
  if (b < a) l = b;
  int l2 = c;
  if (d < c) l2 = c;
  return min(l, l2);
}

inline int max4(int a, int b, int c, int d) {
  int l = a;
  if (b > a) l = b;
  int l2 = c;
  if (d > c) l2 = d;
  return max(l, l2);
}

struct State {
  int s,t,u,v;
  State() : s(), t(), u(), v() {}
  State(int a, int b, int c, int d) : s(a), t(b), u(c), v(d) {
    assert(s <= t);
    assert(u <= v);
  }
  bool IsGood() const {
    return (s != 0 || t != 0 || u != 0 || v != 0);
  }
  CombinationType operator&(const State& r) const {
    if (r.s != t) return kNONE;
    if (v <= r.u) return kMONO;
    if (r.v <= u) return kSWAP;
    if (v >= r.v && u <= r.u) return kCONTAINS_R;
    if (r.v >= v && r.u <= u) return kCONTAINS_L;
    return kINTERLEAVE;
  }
  State& operator*=(const State& r) {
    assert(r.s == t);
    t = r.t;
    const int tu = min4(u, v, r.u, r.v);
    v = max4(u, v, r.u, r.v);
    u = tu;
    return *this;
  }
};

double score(CombinationType x) {
  switch (x) {
    case kNONE: return 0.0;
    case kAXIOM: return 1.0;
    case kMONO: return 16.0;
    case kSWAP: return  8.0;
    case kCONTAINS_R: return 4.0;
    case kCONTAINS_L: return 2.0;
    case kINTERLEAVE: return 1.0;
  }
}

State operator*(const State& l, const State& r) {
  State res = l;
  res *= r;
  return res;
}

ostream& operator<<(ostream& os, const State& s) {
  return os << '[' << s.s << ", " << s.t << ", " << s.u << ", " << s.v << ']';
}

string NT(const State& s) {
  bool decorate=true;
  if (decorate) {
    ostringstream os;
    os << "[X_" << s.s << '_' << s.t << '_' << s.u << '_' << s.v << "]";
    return os.str();
  } else {
    return "[X]";
  }
}

void CreateEdge(const vector<WordID>& f, const vector<WordID>& e, CombinationType ct, const State& cur, const State& left, const State& right) {
  switch(ct) {
    case kINTERLEAVE:
    case kAXIOM:
      cerr << NT(cur) << " ||| " << Substring(f, cur.s, cur.t) << " ||| " << Substring(e, cur.u, cur.v) << "\n";
      break;
    case kMONO:
      cerr << NT(cur) << " ||| " << NT(left) << ' ' << NT(right) << " ||| [1] [2]\n";
      break;
    case kSWAP:
      cerr << NT(cur) << " ||| " << NT(left) << ' ' << NT(right) << " ||| [2] [1]\n";
      break;
    case kCONTAINS_L:
      cerr << NT(cur) << " ||| " << Substring(f, right.s, left.s) << ' ' << NT(left) << ' ' << Substring(f, left.t, right.t) << " ||| " << Substring(e, right.u, left.u) << " [1] " << Substring(e, left.v, right.v) << endl;
      break;
    case kCONTAINS_R:
      cerr << NT(cur) << " ||| " << Substring(f, left.s, right.s) << ' ' << NT(right) << ' ' << Substring(f, right.t, left.t) << " ||| " << Substring(e, left.u, right.u) << " [1] " << Substring(e, right.v, left.v) << endl;
      break;
    }
}

void BuildArity2Forest(const vector<WordID>& f, const vector<WordID>& e, const vector<State>& axioms) {
  const unsigned n = f.size();
  Array2D<State> chart(n, n+1);
  Array2D<CombinationType> ctypes(n, n+1);
  Array2D<double> cscore(n, n+1);
  Array2D<int> cmids(n, n+1, -1);
  for (const auto& axiom : axioms) {
    chart(axiom.s, axiom.t) = axiom;
    ctypes(axiom.s, axiom.t) = kAXIOM;
    cscore(axiom.s, axiom.t) = 1.0;
    CreateEdge(f, e, kAXIOM, axiom, axiom, axiom);
    //cerr << "AXIOM " << axiom.s << ", " << axiom.t << " : " << chart(axiom.s, axiom.t) << " : " << 1 << endl;
  }
  for (unsigned l = 2; l <= n; ++l) {
    const unsigned i_end = n + 1 - l;
    for (unsigned i = 0; i < i_end; ++i) {
      const unsigned j = i + l;
      for (unsigned k = i + 1; k < j; ++k) {
        const State& left = chart(i, k);
        const State& right = chart(k, j);
        if (!left.IsGood() || !right.IsGood()) continue;
        CombinationType comb = left & right;
        if (comb != kNONE) {
          double ns = cscore(i,k) + cscore(k,j) + score(comb);
          if (ns > cscore(i,j)) {
            cscore(i,j) = ns;
            chart(i,j) = left * right;
            cmids(i,j) = k;
            ctypes(i,j) = comb;
            //cerr << "PROVED " << chart(i,j) << " : " << cscore(i,j) << "  [" << nm(comb) << " " << left << " * " << right << "]\n";
          } else {
            //cerr << "SUBOPTIMAL " << (left*right) << " : " << ns << "  [" << nm(comb) << " " << left << " * " << right << "]\n";
          }
          CreateEdge(f, e, comb, left * right, left, right);
        } else {
          //cerr << "CAN'T " << left << " * " << right << endl;
        }
      }
    }
  }
}

int main(int argc, char** argv) {
  State s;
  vector<WordID> e,f;
  TD::ConvertSentence("B C that A", &e);
  TD::ConvertSentence("A de B C", &f);
  State w0(0,1,3,4), w1(1,2,2,3), w2(2,3,0,1), w3(3,4,1,2);
  vector<State> al = {w0, w1, w2, w3};
  // f cannot have any unaligned words, however, multiple overlapping axioms are possible
  // so you can write code to align unaligned words in all ways to surrounding words
  BuildArity2Forest(f, e, al);

  TD::ConvertSentence("A B C D", &e);
  TD::ConvertSentence("A B , C D", &f);
  vector<State> al2 = {State(0,1,0,1), State(1,2,1,2), State(1,3,1,2), State(2,4,2,3), State(4,5,3,4)};
  BuildArity2Forest(f, e, al2);

  TD::ConvertSentence("A B C D", &e);
  TD::ConvertSentence("C A D B", &f);
  vector<State> al3 = {State(0,1,2,3), State(1,2,0,1), State(2,3,3,4), State(3,4,1,2)};
  BuildArity2Forest(f, e, al3);

  // things to do: run EM, do posterior inference with a Dirichlet prior, etc.
  return 0;
}

