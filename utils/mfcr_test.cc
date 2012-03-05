#include "mfcr.h"

#include <iostream>
#include <cassert>
#include <cmath>

#include "sampler.h"

using namespace std;

void test_exch(MT19937* rng) {
  MFCR<2, int> crp(0.5, 3.0);
  vector<double> lambdas(2);
  vector<double> p0s(2);
  lambdas[0] = 0.2;
  lambdas[1] = 0.8;
  p0s[0] = 1.0;
  p0s[1] = 1.0;

  double tot = 0;
  double tot2 = 0;
  double xt = 0;
  int cust = 10;
  vector<int> hist(cust + 1, 0), hist2(cust + 1, 0);
  for (int i = 0; i < cust; ++i) { crp.increment(1, p0s.begin(), lambdas.begin(), rng); }
  const int samples = 100000;
  const bool simulate = true;
  for (int k = 0; k < samples; ++k) {
    if (!simulate) {
      crp.clear();
      for (int i = 0; i < cust; ++i) { crp.increment(1, p0s.begin(), lambdas.begin(), rng); }
    } else {
      int da = rng->next() * cust;
      bool a = rng->next() < 0.45;
      if (a) {
        for (int i = 0; i < da; ++i) { crp.increment(1, p0s.begin(), lambdas.begin(), rng); }
        for (int i = 0; i < da; ++i) { crp.decrement(1, rng); }
        xt += 1.0;
      } else {
        for (int i = 0; i < da; ++i) { crp.decrement(1, rng); }
        for (int i = 0; i < da; ++i) { crp.increment(1, p0s.begin(), lambdas.begin(), rng); }
      }
    }
    int c = crp.num_tables(1);
    ++hist[c];
    tot += c;
    int c2 = crp.num_tables(1,0);  // tables on floor 0 with dish 1
    ++hist2[c2];
    tot2 += c2;
  }
  cerr << cust << " = " << crp.num_customers() << endl;
  cerr << "P(a) = " << (xt / samples) << endl;
  cerr << "E[num tables] = " << (tot / samples) << endl;
  double error = fabs((tot / samples) - 6.894);
  cerr << "   error = " << error << endl;
  for (int i = 1; i <= cust; ++i)
    cerr << i << ' ' << (hist[i]) << endl;
  cerr << "E[num tables on floor 0] = " << (tot2 / samples) << endl;
  double error2 = fabs((tot2 / samples) - 1.379);
  cerr << "  error2 = " << error2 << endl;
  for (int i = 1; i <= cust; ++i)
    cerr << i << ' ' << (hist2[i]) << endl;
  assert(error < 0.05);   // these can fail with very low probability
  assert(error2 < 0.05);
};

int main(int argc, char** argv) {
  MT19937 rng;
  test_exch(&rng);
  return 0;
}

