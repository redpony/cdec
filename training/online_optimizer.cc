#include "online_optimizer.h"

LearningRateSchedule::~LearningRateSchedule() {}

double StandardLearningRate::eta(int k) const {
  return eta_0_ / (1.0 + k / N_);
}

double ExponentialDecayLearningRate::eta(int k) const {
  return eta_0_ * pow(alpha_, k / N_);
}

OnlineOptimizer::~OnlineOptimizer() {}

void OnlineOptimizer::ResetEpochImpl() {}

