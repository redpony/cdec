#include "cllh_observer.h"

#include <cmath>
#include <cassert>

#include "inside_outside.h"
#include "hg.h"
#include "sentence_metadata.h"

using namespace std;

static const double kMINUS_EPSILON = -1e-6;

ConditionalLikelihoodObserver::~ConditionalLikelihoodObserver() {}

void ConditionalLikelihoodObserver::NotifyDecodingStart(const SentenceMetadata&) {
  cur_obj = 0;
  state = 1;
}

void ConditionalLikelihoodObserver::NotifyTranslationForest(const SentenceMetadata&, Hypergraph* hg) {
  assert(state == 1);
  state = 2;
  SparseVector<prob_t> cur_model_exp;
  const prob_t z = InsideOutside<prob_t,
                                 EdgeProb,
                                 SparseVector<prob_t>,
                                 EdgeFeaturesAndProbWeightFunction>(*hg, &cur_model_exp);
  cur_obj = log(z);
}

void ConditionalLikelihoodObserver::NotifyAlignmentForest(const SentenceMetadata& smeta, Hypergraph* hg) {
  assert(state == 2);
  state = 3;
  SparseVector<prob_t> ref_exp;
  const prob_t ref_z = InsideOutside<prob_t,
                                     EdgeProb,
                                     SparseVector<prob_t>,
                                     EdgeFeaturesAndProbWeightFunction>(*hg, &ref_exp);

  double log_ref_z = log(ref_z);

  // rounding errors means that <0 is too strict
  if ((cur_obj - log_ref_z) < kMINUS_EPSILON) {
    cerr << "DIFF. ERR! log_model_z < log_ref_z: " << cur_obj << " " << log_ref_z << endl;
    exit(1);
  }
  assert(!std::isnan(log_ref_z));
  acc_obj += (cur_obj - log_ref_z);
  trg_words += smeta.GetReference().size();
}

