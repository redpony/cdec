#ifndef _BIN_CLASS_H_
#define _BIN_CLASS_H_

#include <vector>
#include "sparse_vector.h"

struct TrainingInstance {
  // TODO add other info? loss for MIRA-type updates?
  SparseVector<double> x_feature_map;
  bool y;
};

struct Objective {
  virtual ~Objective();

  // returns f(x) and f'(x)
  virtual double ObjectiveAndGradient(const SparseVector<double>& x,
                  const std::vector<TrainingInstance>& training_instances,
                  SparseVector<double>* g) const = 0;
};

#endif
