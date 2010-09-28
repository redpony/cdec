#ifndef _ONL_OPTIMIZE_H_
#define _ONL_OPTIMIZE_H_

#include <tr1/memory>
#include <string>
#include <cmath>
#include "sparse_vector.h"

struct LearningRateSchedule {
  virtual ~LearningRateSchedule();
  // returns the learning rate for the kth iteration
  virtual double eta(int k) const = 0;
};

// TODO in the Tsoruoaka et al. (ACL 2009) paper, they use N
// to mean the batch size in most places, but it doesn't completely
// make sense to me in the learning rate schedules-- this needs
// to be worked out to make sure they didn't mean corpus size
// in some places and batch size in others (since in the paper they
// only ever work with batch sizes of 1)
struct StandardLearningRate : public LearningRateSchedule {
  StandardLearningRate(
      size_t batch_size,        // batch size, not corpus size!
      double eta_0 = 0.2) :
    eta_0_(eta_0),
    N_(static_cast<double>(batch_size)) {}

  virtual double eta(int k) const;

 private:
  const double eta_0_;
  const double N_;
};

struct ExponentialDecayLearningRate : public LearningRateSchedule {
  ExponentialDecayLearningRate(
      size_t batch_size,        // batch size, not corpus size!
      double eta_0 = 0.2,
      double alpha = 0.85       // recommended by Tsuruoka et al. (ACL 2009)
    ) : eta_0_(eta_0),
        N_(static_cast<double>(batch_size)),
        alpha_(alpha) {
    assert(alpha > 0);
    assert(alpha < 1.0);
  }

  virtual double eta(int k) const;

 private:
  const double eta_0_;
  const double N_;
  const double alpha_;
};

class OnlineOptimizer {
 public:
  virtual ~OnlineOptimizer();
  OnlineOptimizer(const std::tr1::shared_ptr<LearningRateSchedule>& s,
                  size_t batch_size)
    : N_(batch_size),schedule_(s),k_() {}
  void UpdateWeights(const SparseVector<double>& approx_g, int max_feat, SparseVector<double>* weights) {
    ++k_;
    const double eta = schedule_->eta(k_);
    UpdateWeightsImpl(eta, approx_g, max_feat, weights);
  }

 protected:
  virtual void UpdateWeightsImpl(const double& eta, const SparseVector<double>& approx_g, int max_feat, SparseVector<double>* weights) = 0;
  const size_t N_; // number of training instances per batch

 private:
  std::tr1::shared_ptr<LearningRateSchedule> schedule_;
  int k_;  // iteration count
};

class CumulativeL1OnlineOptimizer : public OnlineOptimizer {
 public:
  CumulativeL1OnlineOptimizer(const std::tr1::shared_ptr<LearningRateSchedule>& s,
                              size_t training_instances, double C) :
    OnlineOptimizer(s, training_instances), C_(C), u_() {}

 protected:
  void UpdateWeightsImpl(const double& eta, const SparseVector<double>& approx_g, int max_feat, SparseVector<double>* weights) {
    u_ += eta * C_ / N_;
    (*weights) += eta * approx_g;
    for (int i = 1; i < max_feat; ++i)
      ApplyPenalty(i, weights);
  }

 private:
  void ApplyPenalty(int i, SparseVector<double>* w) {
    const double z = w->value(i);
    double w_i = z;
    double q_i = q_.value(i);
    if (w_i > 0.0)
      w_i = std::max(0.0, w_i - (u_ + q_i));
    else if (w_i < 0.0)
      w_i = std::min(0.0, w_i + (u_ - q_i));
    q_i += w_i - z;
    if (q_i == 0.0)
      q_.erase(i);
    else
      q_.set_value(i, q_i);
    if (w_i == 0.0)
      w->erase(i);
    else
      w->set_value(i, w_i);
  }

  const double C_;  // reguarlization strength
  double u_;
  SparseVector<double> q_;
};

#endif
