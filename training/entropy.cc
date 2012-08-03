#include "entropy.h"

#include "prob.h"
#include "candidate_set.h"

using namespace std;

namespace training {

// see Mann and McCallum "Efficient Computation of Entropy Gradient ..." for
// a mostly clear derivation of:
//   g = E[ F(x,y) * log p(y|x) ] + H(y | x) * E[ F(x,y) ]
double CandidateSetEntropy::operator()(const vector<double>& params,
                                       SparseVector<double>* g) const {
  prob_t z;
  vector<double> dps(cands_.size());
  for (unsigned i = 0; i < cands_.size(); ++i) {
    dps[i] = cands_[i].fmap.dot(params);
    const prob_t u(dps[i], init_lnx());
    z += u;
  }
  const double log_z = log(z);

  SparseVector<double> exp_feats;
  double entropy = 0;
  for (unsigned i = 0; i < cands_.size(); ++i) {
    const double log_prob = cands_[i].fmap.dot(params) - log_z;
    const double prob = exp(log_prob);
    const double e_logprob = prob * log_prob;
    entropy -= e_logprob;
    if (g) {
      (*g) += cands_[i].fmap * e_logprob;
      exp_feats += cands_[i].fmap * prob;
    }
  }
  if (g) (*g) += exp_feats * entropy;
  return entropy;
}

}

