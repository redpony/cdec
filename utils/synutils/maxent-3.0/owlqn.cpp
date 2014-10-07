#include <vector>
#include <iostream>
#include <cmath>
#include <stdio.h>
#include "mathvec.h"
#include "lbfgs.h"
#include "maxent.h"

using namespace std;

const static int    M = LBFGS_M;
const static double LINE_SEARCH_ALPHA = 0.1;
const static double LINE_SEARCH_BETA  = 0.5;

// stopping criteria
int    OWLQN_MAX_ITER      = 300;
const static double MIN_GRAD_NORM = 0.0001;


Vec approximate_Hg(const int iter, const Vec & grad,
		   const Vec s[],  const Vec y[], const double z[]);


inline int sign(double x) 
{
  if (x > 0) return 1;
  if (x < 0) return -1;
  return 0;
};

static Vec
pseudo_gradient(const Vec & x, const Vec & grad0, const double C)
{
  Vec grad = grad0;
  for (size_t i = 0; i < x.Size(); i++) {
    if (x[i] != 0) {
      grad[i] += C * sign(x[i]);
      continue;
    }
    const double gm = grad0[i] - C;
    if (gm > 0) {
      grad[i] = gm;
      continue;
    }
    const double gp = grad0[i] + C;
    if (gp < 0) {
      grad[i] = gp;
      continue;
    }
    grad[i] = 0;
  }

  return grad;
}

double 
ME_Model::regularized_func_grad(const double C, const Vec & x, Vec & grad)
{
  double f = FunctionGradient(x.STLVec(), grad.STLVec());
  for (size_t i = 0; i < x.Size(); i++) {
    f += C * fabs(x[i]);
  }

  return f;
}

double 
ME_Model::constrained_line_search(double C,
			const Vec & x0, const Vec & grad0, const double f0, 
			const Vec & dx, Vec & x, Vec & grad1)
{
  // compute the orthant to explore
  Vec orthant = x0;
  for (size_t i = 0; i < orthant.Size(); i++) {
    if (orthant[i] == 0) orthant[i] = -grad0[i];
  }

  double t = 1.0 / LINE_SEARCH_BETA;

  double f;
  do {
    t *= LINE_SEARCH_BETA;
    x = x0 + t * dx;
    x.Project(orthant);
    //    for (size_t i = 0; i < x.Size(); i++) {
    //      if (x0[i] != 0 && sign(x[i]) != sign(x0[i])) x[i] = 0;
    //    }

    f = regularized_func_grad(C, x, grad1);
    //        cout << "*";
  } while (f > f0 + LINE_SEARCH_ALPHA * dot_product(x - x0, grad0));

  return f;
}

vector<double> 
ME_Model::perform_OWLQN(const vector<double> & x0, const double C)
{
  const size_t dim = x0.size();
  Vec x = x0;

  Vec grad(dim), dx(dim);
  double f = regularized_func_grad(C, x, grad);

  Vec s[M], y[M];
  double z[M];  // rho

  for (int iter = 0; iter < OWLQN_MAX_ITER; iter++) {
    Vec pg = pseudo_gradient(x, grad, C);

    fprintf(stderr, "%3d  obj(err) = %f (%6.4f)", iter+1, -f, _train_error);
    if (_nheldout > 0) {
      const double heldout_logl = heldout_likelihood();
      fprintf(stderr, "  heldout_logl(err) = %f (%6.4f)", heldout_logl, _heldout_error);
    }
    fprintf(stderr, "\n");

    if (sqrt(dot_product(pg, pg)) < MIN_GRAD_NORM) break;

    dx = -1 * approximate_Hg(iter, pg, s, y, z);
    if (dot_product(dx, pg) >= 0)
      dx.Project(-1 * pg);

    Vec x1(dim), grad1(dim);
    f = constrained_line_search(C, x, pg, f, dx, x1, grad1);

    s[iter % M] = x1 - x;
    y[iter % M] = grad1 - grad;
    z[iter % M] = 1.0 / dot_product(y[iter % M], s[iter % M]);

    x = x1;
    grad = grad1;
  }

  return x.STLVec();
}

