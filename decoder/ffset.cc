#include "ffset.h"

#include "ff.h"
#include "tdict.h"
#include "hg.h"

using namespace std;

ModelSet::ModelSet(const vector<double>& w, const vector<const FeatureFunction*>& models) :
    models_(models),
    weights_(w),
    state_size_(0),
    model_state_pos_(models.size()) {
  for (int i = 0; i < models_.size(); ++i) {
    model_state_pos_[i] = state_size_;
    state_size_ += models_[i]->StateSize();
  }

  //added by lijunhui
  //erase the states for SRLReorderFeature and DepPrnFeatures
  for (int i = 0; i < models_.size(); i++) {
    if (models_[i]->name_ == string("SRLReorderFeature")
      || models_[i]->name_ == string("DepPrnFeatures")
      || models_[i]->name_ == string("SyntacticContextFeature")
      || models_[i]->name_ == string("ArgtReorderFeature")
      || models_[i]->name_ == string("ConstReorderSparseFeature")
      || models_[i]->name_ == string("ConstReorderFeature")) {
      int start_pos = model_state_pos_[i];
      int end_pos;
      if (i == models_.size() - 1)
        end_pos = state_size_;
      else
        end_pos = model_state_pos_[i + 1];
      erase_state_start_pos_.push_back(start_pos);
      erase_state_end_pos_.push_back(end_pos);
    }
  }
}

void ModelSet::PrepareForInput(const SentenceMetadata& smeta) {
  for (int i = 0; i < models_.size(); ++i)
    const_cast<FeatureFunction*>(models_[i])->PrepareForInput(smeta);
}

void ModelSet::AddFeaturesToEdge(const SentenceMetadata& smeta,
                                 const Hypergraph& /* hg */,
                                 const FFStates& node_states,
                                 HG::Edge* edge,
                                 FFState* context,
                                 prob_t* combination_cost_estimate) const {
  //edge->reset_info();
  context->resize(state_size_);
  if (state_size_ > 0) {
    memset(&(*context)[0], 0, state_size_);
  }
  SparseVector<double> est_vals;  // only computed if combination_cost_estimate is non-NULL
  if (combination_cost_estimate) *combination_cost_estimate = prob_t::One();
  for (int i = 0; i < models_.size(); ++i) {
    const FeatureFunction& ff = *models_[i];
    void* cur_ff_context = NULL;
    vector<const void*> ants(edge->tail_nodes_.size());
    bool has_context = ff.StateSize() > 0;
    if (has_context) {
      int spos = model_state_pos_[i];
      cur_ff_context = &(*context)[spos];
      for (int i = 0; i < ants.size(); ++i) {
        ants[i] = &node_states[edge->tail_nodes_[i]][spos];
      }
    }
    ff.TraversalFeatures(smeta, *edge, ants, &edge->feature_values_, &est_vals, cur_ff_context);
  }
  if (combination_cost_estimate)
    combination_cost_estimate->logeq(est_vals.dot(weights_));
  edge->edge_prob_.logeq(edge->feature_values_.dot(weights_));
}

void ModelSet::AddFinalFeatures(const FFState& state, HG::Edge* edge,SentenceMetadata const& smeta) const {
  assert(1 == edge->rule_->Arity());
  //edge->reset_info();
  for (int i = 0; i < models_.size(); ++i) {
    const FeatureFunction& ff = *models_[i];
    const void* ant_state = NULL;
    bool has_context = ff.StateSize() > 0;
    if (has_context) {
      int spos = model_state_pos_[i];
      ant_state = &state[spos];
    }
    ff.FinalTraversalFeatures(ant_state, &edge->feature_values_);
  }
  edge->edge_prob_.logeq(edge->feature_values_.dot(weights_));
}

bool ModelSet::HaveEraseState() const {
       if (erase_state_start_pos_.size() == 0) return false;
       return true;
}

void ModelSet::GetRealFFState(const FFState& state, FFState& real_state) const {
       real_state.resize(state.size());
       for (int i = 0; i < state.size(); i++) {
               real_state[i] = state[i];
       }

       if (state.size() == 0)
               return;
       assert(state.size() == state_size_);

       //erase the states for SRLReorderFeature and DepPrnFeatures and SyntacticContextFeature
       for (int i = 0; i < erase_state_start_pos_.size(); i++){
               int start_pos = erase_state_start_pos_[i];
               int end_pos = erase_state_end_pos_[i];
               for (int j = start_pos; j < end_pos; j++)
                       real_state[j] = 0;
       }
}

FFState ModelSet::GetRealFFState(const FFState& state) const {
       FFState real_state;
       GetRealFFState(state, real_state);
       return real_state;
}
