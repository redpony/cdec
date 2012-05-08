#include <iostream>
#include <liblbfgs/lbfgs++.h>

using namespace std;

// Function must be lbfgsfloatval_t f(x.begin, x.end, g.begin)
lbfgsfloatval_t func(const lbfgsfloatval_t* x, lbfgsfloatval_t* g) {
    int i;
    lbfgsfloatval_t fx = 0.0;
    int n = 4;

    for (i = 0;i < n;i += 2) {
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
  LBFGS<F> lbfgs(4, f, 1.0);
  lbfgs.Optimize();
}

int main(int argc, char** argv) {
  Opt(func);
  return 0;
}

