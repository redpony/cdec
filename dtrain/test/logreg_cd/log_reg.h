#ifndef _LOG_REG_H_
#define _LOG_REG_H_

#include <vector>
#include "sparse_vector.h"
#include "bin_class.h"

struct LogisticRegression : public Objective {
  double ObjectiveAndGradient(const SparseVector<double>& x,
                              const std::vector<TrainingInstance>& training_instances,
                              SparseVector<double>* g) const;
};

#endif
