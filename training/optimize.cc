#include "optimize.h"

#include <iostream>
#include <cassert>

#include "lbfgs.h"

using namespace std;

BatchOptimizer::~BatchOptimizer() {}

void BatchOptimizer::Save(ostream* out) const {
  out->write((const char*)&eval_, sizeof(eval_));
  out->write((const char*)&has_converged_, sizeof(has_converged_));
  SaveImpl(out);
  unsigned int magic = 0xABCDDCBA;  // should be uint32_t
  out->write((const char*)&magic, sizeof(magic));
}

void BatchOptimizer::Load(istream* in) {
  in->read((char*)&eval_, sizeof(eval_));
  in->read((char*)&has_converged_, sizeof(has_converged_));
  LoadImpl(in);
  unsigned int magic = 0;           // should be uint32_t
  in->read((char*)&magic, sizeof(magic));
  assert(magic == 0xABCDDCBA);
  cerr << Name() << " EVALUATION #" << eval_ << endl;
}

void BatchOptimizer::SaveImpl(ostream* out) const {
  (void)out;
}

void BatchOptimizer::LoadImpl(istream* in) {
  (void)in;
}

string RPropOptimizer::Name() const {
  return "RPropOptimizer";
}

void RPropOptimizer::OptimizeImpl(const double& obj,
                              const vector<double>& g,
                              vector<double>* x) {
  for (int i = 0; i < g.size(); ++i) {
    const double g_i = g[i];
    const double sign_i = (signbit(g_i) ? -1.0 : 1.0);
    const double prod = g_i * prev_g_[i];
    if (prod > 0.0) {
      const double dij = min(delta_ij_[i] * eta_plus_, delta_max_);
      (*x)[i] -= dij * sign_i;
      delta_ij_[i] = dij;
      prev_g_[i] = g_i;
    } else if (prod < 0.0) {
      delta_ij_[i] = max(delta_ij_[i] * eta_minus_, delta_min_);
      prev_g_[i] = 0.0;
    } else {
      (*x)[i] -= delta_ij_[i] * sign_i;
      prev_g_[i] = g_i;
    }
  }
}

void RPropOptimizer::SaveImpl(ostream* out) const {
  const size_t n = prev_g_.size();
  out->write((const char*)&n, sizeof(n));
  out->write((const char*)&prev_g_[0], sizeof(double) * n);
  out->write((const char*)&delta_ij_[0], sizeof(double) * n);
}

void RPropOptimizer::LoadImpl(istream* in) {
  size_t n;
  in->read((char*)&n, sizeof(n));
  assert(n == prev_g_.size());
  assert(n == delta_ij_.size());
  in->read((char*)&prev_g_[0], sizeof(double) * n);
  in->read((char*)&delta_ij_[0], sizeof(double) * n);
}

string LBFGSOptimizer::Name() const {
  return "LBFGSOptimizer";
}

LBFGSOptimizer::LBFGSOptimizer(int num_feats, int memory_buffers) :
  opt_(num_feats, memory_buffers) {}

void LBFGSOptimizer::SaveImpl(ostream* out) const {
  opt_.serialize(out);
}

void LBFGSOptimizer::LoadImpl(istream* in) {
  opt_.deserialize(in);
}

void LBFGSOptimizer::OptimizeImpl(const double& obj,
                                  const vector<double>& g,
                                  vector<double>* x) {
  opt_.run(&(*x)[0], obj, &g[0]);
  if (!opt_.requests_f_and_g()) opt_.run(&(*x)[0], obj, &g[0]);
  // cerr << opt_ << endl;
}

