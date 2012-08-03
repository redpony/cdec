#include "risk.h"

#include "prob.h"
#include "candidate_set.h"
#include "ns.h"

using namespace std;

namespace training {

// g = \sum_e p(e|f) * loss(e) * (phi(e,f) - E[phi(e,f)])
double CandidateSetRisk::operator()(const vector<double>& params,
                                    SparseVector<double>* g) const {
  prob_t z;
  for (unsigned i = 0; i < cands_.size(); ++i) {
    const prob_t u(cands_[i].fmap.dot(params), init_lnx());
    z += u;
  }
  const double log_z = log(z);

  SparseVector<double> exp_feats;
  if (g) {
    for (unsigned i = 0; i < cands_.size(); ++i) {
      const double log_prob = cands_[i].fmap.dot(params) - log_z;
      const double prob = exp(log_prob);
      exp_feats += cands_[i].fmap * prob;
    }
  }

  double risk = 0;
  for (unsigned i = 0; i < cands_.size(); ++i) {
    const double log_prob = cands_[i].fmap.dot(params) - log_z;
    const double prob = exp(log_prob);
    const double cost = metric_.IsErrorMetric() ? metric_.ComputeScore(cands_[i].eval_feats)
                                                : 1.0 - metric_.ComputeScore(cands_[i].eval_feats);
    const double r = prob * cost;
    risk += r;
    if (g) (*g) += (cands_[i].fmap - exp_feats) * r;
  }
  return risk;
}

}


