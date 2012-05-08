// THIS IS CDEC'S C++ WRAPPER AROUND LIBLBFGS
// liblbfgs is
//                                       Copyright (c) 1990, Jorge Nocedal
//                                 Copyright (c) 2007-2010, Naoaki Okazaki
//
// see https://github.com/chokkan/liblbfgs for more details
//
#ifndef __LBFGSPP_H__
#define __LBFGSPP_H__

#include <vector>
#include "liblbfgs/lbfgs.h"

// Function must be lbfgsfloatval_t f(const double* x_start, double* g_start)
template <typename Function>
class LBFGS {
 public:
  LBFGS(size_t n,            // number of variables
        const Function& f,   // function to optimize
        double l1_c = 0.0,   // l1 penalty strength
        size_t m = 10        // number of memory buffers
                             // TODO should use custom allocator here:
        ) : p_x(new std::vector<lbfgsfloatval_t>(n, 0.0)),
                             owned(true),
                             m_x(*p_x),
                             func(f) {
    Init(m, l1_c);
  }

  // constructor where external vector storage for variables is used
  LBFGS(std::vector<lbfgsfloatval_t>* px,
        const Function& f,
        double l1_c = 0.0,   // l1 penalty strength
        size_t m = 10
        ) : p_x(px),
                             owned(false),
                             m_x(*p_x),
                             func(f) {
    Init(m, l1_c);
  }

  ~LBFGS() {
    if (owned) delete p_x;
  }
  const lbfgsfloatval_t& operator[](size_t i) const { return m_x[i]; }
  lbfgsfloatval_t& operator[](size_t i) { return m_x[i]; }
  size_t size() const { return m_x.size(); }

  int Optimize() {
    lbfgsfloatval_t fx;
    int ret = lbfgs(m_x.size(), &m_x[0], &fx, _evaluate, _progress, this, &param);
    std::cerr << "L-BFGS optimization terminated with status code = " << ret << std::endl;
    std::cerr << "  fx = " << fx << std::endl;
    return ret;
  }

 private:
  void Init(size_t m, double l1_c) {
    lbfgs_parameter_init(&param);
    param.m = m;
    if (l1_c > 0.0) {
      param.linesearch = LBFGS_LINESEARCH_BACKTRACKING;
      param.orthantwise_c = 1.0;
    }
  }

  static lbfgsfloatval_t _evaluate(
        void *instance,
        const lbfgsfloatval_t *x,
        lbfgsfloatval_t *g,
        const int n,
        const lbfgsfloatval_t step) {
      return reinterpret_cast<LBFGS<Function>*>(instance)->evaluate(x, g, n, step);
    }

    lbfgsfloatval_t evaluate(const lbfgsfloatval_t *x,
                             lbfgsfloatval_t *g,
                             const int n,
                             const lbfgsfloatval_t step) {
      (void) n;
      (void) step;
      return func(x, g);
    }

    static int _progress(
        void *instance,
        const lbfgsfloatval_t *x,
        const lbfgsfloatval_t *g,
        const lbfgsfloatval_t fx,
        const lbfgsfloatval_t xnorm,
        const lbfgsfloatval_t gnorm,
        const lbfgsfloatval_t step,
        int n,
        int k,
        int ls
        )
    {
        return reinterpret_cast<LBFGS<Function>*>(instance)
          ->progress(x, g, fx, xnorm, gnorm, step, n, k, ls);
    }

    int progress(
        const lbfgsfloatval_t *x,
        const lbfgsfloatval_t *g,
        const lbfgsfloatval_t fx,
        const lbfgsfloatval_t xnorm,
        const lbfgsfloatval_t gnorm,
        const lbfgsfloatval_t step,
        int n,
        int k,
        int ls
        )
    {
        (void) n;
        (void) k;
        std::cerr << "Iteration " << k << ':' << std::endl;
        std::cerr << "  fx = " << fx << ", x[0] = " << x[0] << ", x[1] = " << x[1] << std::endl;
        std::cerr << "  xnorm = " << xnorm << ", gnorm = " << gnorm << ", step = " << step << std::endl << std::endl;
        return 0;
    }
  std::vector<lbfgsfloatval_t>* p_x;
  const bool owned;
  std::vector<lbfgsfloatval_t>& m_x;
  const Function& func;
  lbfgs_parameter_t param;

};

#endif
