#ifndef _OPTIMIZE_H_
#define _OPTIMIZE_H_

#include <iostream>
#include <vector>
#include <string>
#include <cassert>

#include "lbfgs.h"

// abstract base class for first order optimizers
// order of invocation: new, Load(), Optimize(), Save(), delete
class BatchOptimizer {
 public:
  BatchOptimizer() : eval_(1), has_converged_(false) {}
  virtual ~BatchOptimizer();
  virtual std::string Name() const = 0;
  int EvaluationCount() const { return eval_; }
  bool HasConverged() const { return has_converged_; }

  void Optimize(const double& obj,
                const std::vector<double>& g,
                std::vector<double>* x) {
    assert(g.size() == x->size());
    ++eval_;
    OptimizeImpl(obj, g, x);
    scitbx::lbfgs::traditional_convergence_test<double> converged(g.size());
    has_converged_ = converged(&(*x)[0], &g[0]);
  }

  void Save(std::ostream* out) const;
  void Load(std::istream* in);
 protected:
  virtual void SaveImpl(std::ostream* out) const;
  virtual void LoadImpl(std::istream* in);
  virtual void OptimizeImpl(const double& obj,
                            const std::vector<double>& g,
                            std::vector<double>* x) = 0;

  int eval_;
 private:
  bool has_converged_;
};

class RPropOptimizer : public BatchOptimizer {
 public:
  explicit RPropOptimizer(int num_vars,
                          double eta_plus = 1.2,
                          double eta_minus = 0.5,
                          double delta_0 = 0.1,
                          double delta_max = 50.0,
                          double delta_min = 1e-6) :
      prev_g_(num_vars, 0.0),
      delta_ij_(num_vars, delta_0),
      eta_plus_(eta_plus),
      eta_minus_(eta_minus),
      delta_max_(delta_max),
      delta_min_(delta_min) {
    assert(eta_plus > 1.0);
    assert(eta_minus > 0.0 && eta_minus < 1.0);
    assert(delta_max > 0.0);
    assert(delta_min > 0.0);
  }
  std::string Name() const;
  void OptimizeImpl(const double& obj,
                    const std::vector<double>& g,
                    std::vector<double>* x);
  void SaveImpl(std::ostream* out) const;
  void LoadImpl(std::istream* in);
 private:
  std::vector<double> prev_g_;
  std::vector<double> delta_ij_;
  const double eta_plus_;
  const double eta_minus_;
  const double delta_max_;
  const double delta_min_;
};

class LBFGSOptimizer : public BatchOptimizer {
 public:
  explicit LBFGSOptimizer(int num_vars, int memory_buffers = 10);
  std::string Name() const;
  void SaveImpl(std::ostream* out) const;
  void LoadImpl(std::istream* in);
  void OptimizeImpl(const double& obj,
                    const std::vector<double>& g,
                    std::vector<double>* x);
 private:
  scitbx::lbfgs::minimizer<double> opt_;
};

#endif
