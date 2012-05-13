#include <iostream>
#include <liblbfgs/lbfgs++.h>

using namespace std;

// Function must be lbfgsfloatval_t f(x.begin, x.end, g.begin)
lbfgsfloatval_t func(const vector<lbfgsfloatval_t>& x, lbfgsfloatval_t* g) {
    unsigned i;
    lbfgsfloatval_t fx = 0.0;

    for (i = 0;i < x.size();i += 2) {
        lbfgsfloatval_t t1 = 1.0 - x[i];
        lbfgsfloatval_t t2 = 10.0 * (x[i+1] - x[i] * x[i]);
        g[i+1] = 20.0 * t2;
        g[i] = -2.0 * (x[i] * g[i+1] + t1);
        fx += t1 * t1 + t2 * t2;
    }
    return fx;
}

template<typename F>
void Opt(F& f) {
  LBFGS<F> lbfgs(4, f);
  lbfgs.MinimizeFunction();
}

int main() {
  Opt(func);
  return 0;
}

