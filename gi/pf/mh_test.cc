#include "ccrp.h"

#include <vector>
#include <iostream>

#include "tdict.h"
#include "transliterations.h"

using namespace std;

MT19937 rng;

static bool verbose = false;

struct Model {

  Model() : bp(), base(0.2, 0.6) , ccrps(5, CCRP<int>(0.8, 0.5)) {}

  double p0(int x) const {
    assert(x > 0);
    assert(x < 5);
    return 1.0/4.0;
  }

  double llh() const {
    double lh = bp + base.log_crp_prob();
    for (int ctx = 1; ctx < 5; ++ctx)
      lh += ccrps[ctx].log_crp_prob();
    return lh;
  }

  double prob(int ctx, int x) const {
    assert(ctx > 0 && ctx < 5);
    return ccrps[ctx].prob(x, base.prob(x, p0(x)));
  }

  void increment(int ctx, int x) {
    assert(ctx > 0 && ctx < 5);
    if (ccrps[ctx].increment(x, base.prob(x, p0(x)), &rng)) {
      if (base.increment(x, p0(x), &rng)) {
        bp += log(1.0 / 4.0);
      }
    }
  }

  // this is just a biased estimate
  double est_base_prob(int x) {
    return (x + 1) * x / 40.0;
  }

  void increment_is(int ctx, int x) {
    assert(ctx > 0 && ctx < 5);
    SampleSet<double> ss;
    const int PARTICLES = 25;
    vector<CCRP<int> > s1s(PARTICLES, CCRP<int>(0.5,0.5));
    vector<CCRP<int> > sbs(PARTICLES, CCRP<int>(0.5,0.5));
    vector<double> sp0s(PARTICLES);

    CCRP<int> s1 = ccrps[ctx];
    CCRP<int> sb = base;
    double sp0 = bp;
    for (int pp = 0; pp < PARTICLES; ++pp) {
      if (pp > 0) {
        ccrps[ctx] = s1;
        base = sb;
        bp = sp0;
      }

      double q = 1;
      double gamma = 1;
      double est_p = est_base_prob(x);
      //base.prob(x, p0(x)) + rng.next() * 0.1;
      if (ccrps[ctx].increment(x, est_p, &rng, &q)) {
        gamma = q * base.prob(x, p0(x));
        q *= est_p;
        if (verbose) cerr << "(DP-base draw) ";
        double qq = -1;
        if (base.increment(x, p0(x), &rng, &qq)) {
          if (verbose) cerr << "(G0 draw) ";
          bp += log(p0(x));
          qq *= p0(x);
        }
      } else { gamma = q; }
      double w = gamma / q;
      if (verbose)
        cerr << "gamma=" << gamma << " q=" << q << "\tw=" << w << endl;
      ss.add(w);
      s1s[pp] = ccrps[ctx];
      sbs[pp] = base;
      sp0s[pp] = bp;
    }
    int ps = rng.SelectSample(ss);
    ccrps[ctx] = s1s[ps];
    base = sbs[ps];
    bp = sp0s[ps];
    if (verbose) {
      cerr << "SELECTED: " << ps << endl;
      static int cc = 0; cc++; if (cc ==10) exit(1);
    }
  }

  void decrement(int ctx, int x) {
    assert(ctx > 0 && ctx < 5);
    if (ccrps[ctx].decrement(x, &rng)) {
      if (base.decrement(x, &rng)) {
        bp -= log(p0(x));
      }
    }
  }

  double bp;
  CCRP<int> base;
  vector<CCRP<int> > ccrps;

};

int main(int argc, char** argv) {
  if (argc > 1) { verbose = true; }
  vector<int> counts(15, 0);
  vector<int> tcounts(15, 0);
  int points[] = {1,2, 2,2, 3,2, 4,1, 3, 4, 3, 3, 2, 3, 4, 1, 4, 1, 3, 2, 1, 3, 1, 4, 0, 0};
  double tlh = 0;
  double tt = 0;
  for (int n = 0; n < 1000; ++n) {
    if (n % 10 == 0) cerr << '.';
    if ((n+1) % 400 == 0) cerr << " [" << (n+1) << "]\n";
    Model m;
    for (int *x = points; *x; x += 2)
      m.increment(x[0], x[1]);

    for (int j = 0; j < 24; ++j) {
      for (int *x = points; *x; x += 2) {
        if (rng.next() < 0.8) {
          m.decrement(x[0], x[1]);
          m.increment_is(x[0], x[1]);
        }
      }
    }
    counts[m.base.num_customers()]++;
    tcounts[m.base.num_tables()]++;
    tlh += m.llh();
    tt += 1.0;
  }
  cerr << "mean LLH = " << (tlh / tt) << endl;
  for (int i = 0; i < 15; ++i)
    cerr << i << ": " << (counts[i] / tt) << "\t" << (tcounts[i] / tt) << endl;
}

