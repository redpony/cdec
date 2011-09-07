#include "log_reg.h"

#include <vector>
#include <cmath>

#include "sparse_vector.h"

using namespace std;

double LogisticRegression::ObjectiveAndGradient(const SparseVector<double>& x,
                              const vector<TrainingInstance>& training_instances,
                              SparseVector<double>* g) const {
  double cll = 0;
  for (int i = 0; i < training_instances.size(); ++i) {
    const double dotprod = training_instances[i].x_feature_map.dot(x); // TODO no bias, if bias, add x[0]
    double lp_false = dotprod;
    double lp_true = -dotprod;
    if (0 < lp_true) {
      lp_true += log1p(exp(-lp_true));
      lp_false = log1p(exp(lp_false));
    } else {
      lp_true = log1p(exp(lp_true));
      lp_false += log1p(exp(-lp_false));
    }
    lp_true *= -1;
    lp_false *= -1;
    if (training_instances[i].y) {  // true label
      cll -= lp_true;
      (*g) -= training_instances[i].x_feature_map * exp(lp_false);
      // (*g)[0] -= exp(lp_false); // bias
    } else {                  // false label
      cll -= lp_false;
      (*g) += training_instances[i].x_feature_map * exp(lp_true);
      // g += corpus[i].second * exp(lp_true);
    }
  }
  return cll;
}

