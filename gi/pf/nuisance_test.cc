#include "ccrp.h"

#include <vector>
#include <iostream>

#include "tdict.h"
#include "transliterations.h"

using namespace std;

MT19937 rng;

ostream& operator<<(ostream&os, const vector<int>& v) {
  os << '[' << v[0];
  if (v.size() == 2) os << ' ' << v[1];
  return os << ']';
}

struct Base {
  Base() : llh(), v(2), v1(1), v2(1), crp(0.25, 0.5) {}
  inline double p0(const vector<int>& x) const {
    double p = 0.75;
    if (x.size() == 2) p = 0.25;
    p *= 1.0 / 3.0;
    if (x.size() == 2) p *= 1.0 / 3.0;
    return p;
  }
  double est_deriv_prob(int a, int b, int seg) const {
    assert(a > 0 && a < 4);  // a \in {1,2,3}
    assert(b > 0 && b < 4);  // b \in {1,2,3}
    assert(seg == 0 || seg == 1);   // seg \in {0,1}
    if (seg == 0) {
      v[0] = a;
      v[1] = b;
      return crp.prob(v, p0(v));
    } else {
      v1[0] = a;
      v2[0] = b;
      return crp.prob(v1, p0(v1)) * crp.prob(v2, p0(v2));
    }
  }
  double est_marginal_prob(int a, int b) const {
    return est_deriv_prob(a,b,0) + est_deriv_prob(a,b,1);
  }
  int increment(int a, int b, double* pw = NULL) {
    double p1 = est_deriv_prob(a, b, 0);
    double p2 = est_deriv_prob(a, b, 1);
    //p1 = 0.5; p2 = 0.5;
    int seg = rng.SelectSample(p1,p2);
    double tmp = 0;
    if (!pw) pw = &tmp;
    double& w = *pw;
    if (seg == 0) {
      v[0] = a;
      v[1] = b;
      w = crp.prob(v, p0(v)) / p1;
      if (crp.increment(v, p0(v), &rng)) {
        llh += log(p0(v));
      }
    } else {
      v1[0] = a;
      w = crp.prob(v1, p0(v1)) / p2;
      if (crp.increment(v1, p0(v1), &rng)) {
        llh += log(p0(v1));
      }
      v2[0] = b;
      w *= crp.prob(v2, p0(v2));
      if (crp.increment(v2, p0(v2), &rng)) {
        llh += log(p0(v2));
      }
    }
    return seg;
  }
  void increment(int a, int b, int seg) {
    if (seg == 0) {
      v[0] = a;
      v[1] = b;
      if (crp.increment(v, p0(v), &rng)) {
        llh += log(p0(v));
      }
    } else {
      v1[0] = a;
      if (crp.increment(v1, p0(v1), &rng)) {
        llh += log(p0(v1));
      }
      v2[0] = b;
      if (crp.increment(v2, p0(v2), &rng)) {
        llh += log(p0(v2));
      }
    }
  }
  void decrement(int a, int b, int seg) {
    if (seg == 0) {
      v[0] = a;
      v[1] = b;
      if (crp.decrement(v, &rng)) {
        llh -= log(p0(v));
      }
    } else {
      v1[0] = a;
      if (crp.decrement(v1, &rng)) {
        llh -= log(p0(v1));
      }
      v2[0] = b;
      if (crp.decrement(v2, &rng)) {
        llh -= log(p0(v2));
      }
    }
  }
  double log_likelihood() const {
    return llh + crp.log_crp_prob();
  }
  double llh;
  mutable vector<int> v, v1, v2;
  CCRP<vector<int> > crp;
};

int main(int argc, char** argv) {
  double tl = 0;
  const int ITERS = 1000;
  const int PARTICLES = 20;
  const int DATAPOINTS = 50;
  WordID x = TD::Convert("souvenons");
  WordID y = TD::Convert("remember");
  vector<WordID> src; TD::ConvertSentence("s o u v e n o n s", &src);
  vector<WordID> trg; TD::ConvertSentence("r e m e m b e r", &trg);
//  Transliterations xx;
//  xx.Initialize(x, src, y, trg);
//  return 1;

 for (int j = 0; j < ITERS; ++j) {
  Base b;
  vector<int> segs(DATAPOINTS);
  SampleSet<double> ss;
  vector<int> sss;
  for (int i = 0; i < DATAPOINTS; i++) {
    ss.clear();
    sss.clear();
    int x = ((i / 10) % 3) + 1;
    int y = (i % 3) + 1;
    //double ep = b.est_marginal_prob(x,y);
    //cerr << "est p(" << x << "," << y << ") = " << ep << endl;
    for (int n = 0; n < PARTICLES; ++n) {
      double w;
      int seg = b.increment(x,y,&w);
      //cerr << seg << " w=" << w << endl;
      ss.add(w);
      sss.push_back(seg);
      b.decrement(x,y,seg);
    }
    int seg = sss[rng.SelectSample(ss)];
    b.increment(x, y, seg);
    //cerr << "Selected: " << seg << endl;
    //return 1;
    segs[i] = seg;
  }
  tl += b.log_likelihood();
 }
  cerr << "LLH=" << tl / ITERS << endl;
}

