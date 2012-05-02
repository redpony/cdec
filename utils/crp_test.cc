#include <iostream>
#include <vector>
#include <string>

#define BOOST_TEST_MODULE CrpTest
#include <boost/test/unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp>

#include "ccrp.h"
#include "sampler.h"

using namespace std;

MT19937 rng;

BOOST_AUTO_TEST_CASE(Dist) {
  CCRP<string> crp(0.1, 5);
  double un = 0.25;
  int tt = 0;
  tt += crp.increment("hi", un, &rng);
  tt += crp.increment("foo", un, &rng);
  tt += crp.increment("bar", un, &rng);
  tt += crp.increment("bar", un, &rng);
  tt += crp.increment("bar", un, &rng);
  tt += crp.increment("bar", un, &rng);
  tt += crp.increment("bar", un, &rng);
  tt += crp.increment("bar", un, &rng);
  tt += crp.increment("bar", un, &rng);
  cout << "tt=" << tt << endl;
  cout << crp << endl;
  cout << "  P(bar)=" << crp.prob("bar", un) << endl;
  cout << "  P(hi)=" << crp.prob("hi", un) << endl;
  cout << "  P(baz)=" << crp.prob("baz", un) << endl;
  cout << "  P(foo)=" << crp.prob("foo", un) << endl;
  double x = crp.prob("bar", un) + crp.prob("hi", un) + crp.prob("baz", un) + crp.prob("foo", un);
  cout << "    tot=" << x << endl;
  BOOST_CHECK_CLOSE(1.0, x, 1e-6);
  tt += crp.decrement("hi", &rng);
  tt += crp.decrement("bar", &rng);
  cout << crp << endl;
  tt += crp.decrement("bar", &rng);
  cout << crp << endl;
  cout << "tt=" << tt << endl;
}

BOOST_AUTO_TEST_CASE(Exchangability) {
    double tot = 0;
    double xt = 0;
    CCRP<int> crp(0.5, 1.0);
    int cust = 10;
    vector<int> hist(cust + 1, 0);
    for (int i = 0; i < cust; ++i) { crp.increment(1, 1.0, &rng); }
    const int samples = 100000;
    const bool simulate = true;
    for (int k = 0; k < samples; ++k) {
      if (!simulate) {
        crp.clear();
        for (int i = 0; i < cust; ++i) { crp.increment(1, 1.0, &rng); }
      } else {
        int da = rng.next() * cust;
        bool a = rng.next() < 0.5;
        if (a) {
          for (int i = 0; i < da; ++i) { crp.increment(1, 1.0, &rng); }
          for (int i = 0; i < da; ++i) { crp.decrement(1, &rng); }
          xt += 1.0;
        } else {
          for (int i = 0; i < da; ++i) { crp.decrement(1, &rng); }
          for (int i = 0; i < da; ++i) { crp.increment(1, 1.0, &rng); }
        }
      }
      int c = crp.num_tables(1);
      ++hist[c];
      tot += c;
    }
    BOOST_CHECK_EQUAL(cust, crp.num_customers());
    cerr << "P(a) = " << (xt / samples) << endl;
    cerr << "E[num tables] = " << (tot / samples) << endl;
    double error = fabs((tot / samples) - 5.4);
    cerr << "  error = " << error << endl;
    BOOST_CHECK_MESSAGE(error < 0.1, "error is too big = " << error);  // it's possible for this to fail, but
                            // very, very unlikely
    for (int i = 1; i <= cust; ++i)
      cerr << i << ' ' << (hist[i]) << endl;
}

BOOST_AUTO_TEST_CASE(LP) {
  CCRP<string> crp(1,1,1,1,0.1,50.0);
  crp.increment("foo", 1.0, &rng);
  cerr << crp.log_crp_prob() << endl;
}

