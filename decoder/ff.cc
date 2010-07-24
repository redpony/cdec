//TODO: non-sparse vector for all feature functions?  modelset applymodels keeps track of who has what features?  it's nice having FF that could generate a handful out of 10000 possible feats, though.

//TODO: actually score rule_feature()==true features once only, hash keyed on rule or modify TRule directly?  need to keep clear in forest which features come from models vs. rules; then rescoring could drop all the old models features at once

#include <boost/lexical_cast.hpp>
#include "ff.h"

#include "tdict.h"
#include "hg.h"

using namespace std;

FeatureFunction::~FeatureFunction() {}


void FeatureFunction::FinalTraversalFeatures(const void* /* ant_state */,
                                             SparseVector<double>* /* features */) const {
}

string FeatureFunction::usage_helper(std::string const& name,std::string const& params,std::string const& details,bool sp,bool sd) {
  string r=name;
  if (sp) {
    r+=": ";
    r+=params;
  }
  if (sd) {
    r+="\n";
    r+=details;
  }
  return r;
}

Features FeatureFunction::single_feature(WordID feat) {
  return Features(1,feat);
}

Features ModelSet::all_features(std::ostream *warn,bool warn0) {
  typedef Features FFS;
  FFS ffs;
#define WARNFF(x) do { if (warn) { *warn << "WARNING: "<< x ; *warn<<endl; } } while(0)
  typedef std::map<WordID,string> FFM;
  FFM ff_from;
  for (unsigned i=0;i<models_.size();++i) {
    FeatureFunction const& ff=*models_[i];
    string const& ffname=ff.name;
    FFS si=ff.features();
    if (si.empty()) {
      WARNFF(ffname<<" doesn't yet report any feature IDs - implement features() method?");
    }
    unsigned n0=0;
    for (unsigned j=0;j<si.size();++j) {
      WordID fid=si[j];
      if (!fid) ++n0;
      if (fid >= weights_.size())
        weights_.resize(fid+1);
      if (warn0 || fid) {
        pair<FFM::iterator,bool> i_new=ff_from.insert(FFM::value_type(fid,ffname));
        if (i_new.second) {
          if (fid)
            ffs.push_back(fid);
          else
            WARNFF("Feature id 0 for "<<ffname<<" (models["<<i<<"]) - probably no weight provided.  Don't freeze feature ids to see the name");
        } else {
          WARNFF(ffname<<" (models["<<i<<"]) tried to define feature "<<FD::Convert(fid)<<" already defined earlier by "<<i_new.first->second);
        }
      }
    }
    if (n0)
      WARNFF(ffname<<" (models["<<i<<"]) had "<<n0<<" unused features (--no_freeze_feature_set to see them)");
  }
  return ffs;
#undef WARNFF
}

void ModelSet::show_features(std::ostream &out,std::ostream &warn,bool warn_zero_wt)
{
  typedef Features FFS;
  FFS ffs=all_features(&warn,warn_zero_wt);
  out << "Weight  Feature\n";
  for (unsigned i=0;i<ffs.size();++i) {
    WordID fid=ffs[i];
    string const& fname=FD::Convert(fid);
    double wt=weights_[fid];
    if (warn_zero_wt && wt==0)
      warn<<"WARNING: "<<fname<<" has 0 weight."<<endl;
    out << wt << "  " << fname<<endl;
  }

}

// Hiero and Joshua use log_10(e) as the value, so I do to
WordPenalty::WordPenalty(const string& param) :
  fid_(FD::Convert("WordPenalty")),
    value_(-1.0 / log(10)) {
  if (!param.empty()) {
    cerr << "Warning WordPenalty ignoring parameter: " << param << endl;
  }
}

void WordPenalty::TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                        const Hypergraph::Edge& edge,
                                        const std::vector<const void*>& ant_states,
                                        SparseVector<double>* features,
                                        SparseVector<double>* estimated_features,
                                        void* state) const {
  (void) smeta;
  (void) ant_states;
  (void) state;
  (void) estimated_features;
  features->set_value(fid_, edge.rule_->EWords() * value_);
}

SourceWordPenalty::SourceWordPenalty(const string& param) :
    fid_(FD::Convert("SourceWordPenalty")),
    value_(-1.0 / log(10)) {
  if (!param.empty()) {
    cerr << "Warning SourceWordPenalty ignoring parameter: " << param << endl;
  }
}

Features SourceWordPenalty::features() const {
  return single_feature(fid_);
}

Features WordPenalty::features() const {
  return single_feature(fid_);
}


void SourceWordPenalty::TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                        const Hypergraph::Edge& edge,
                                        const std::vector<const void*>& ant_states,
                                        SparseVector<double>* features,
                                        SparseVector<double>* estimated_features,
                                        void* state) const {
  (void) smeta;
  (void) ant_states;
  (void) state;
  (void) estimated_features;
  features->set_value(fid_, edge.rule_->FWords() * value_);
}

ArityPenalty::ArityPenalty(const std::string& param) :
    value_(-1.0 / log(10)) {
  string fname = "Arity_";
  unsigned MAX=DEFAULT_MAX_ARITY;
  using namespace boost;
  if (!param.empty())
    MAX=lexical_cast<unsigned>(param);
  for (unsigned i = 0; i <= MAX; ++i) {
    WordID fid=FD::Convert(fname+lexical_cast<string>(i));
    fids_.push_back(fid);
  }
  while (!fids_.empty() && fids_.back()==0) fids_.pop_back(); // pretty up features vector in case FD was frozen.  doesn't change anything
}

Features ArityPenalty::features() const {
  return Features(fids_.begin(),fids_.end());
}

void ArityPenalty::TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                         const Hypergraph::Edge& edge,
                                         const std::vector<const void*>& ant_states,
                                         SparseVector<double>* features,
                                         SparseVector<double>* estimated_features,
                                         void* state) const {
  (void) smeta;
  (void) ant_states;
  (void) state;
  (void) estimated_features;
  unsigned a=edge.Arity();
  features->set_value(a<fids_.size()?fids_[a]:0, value_);
}

ModelSet::ModelSet(const vector<double>& w, const vector<const FeatureFunction*>& models) :
    models_(models),
    weights_(w),
    state_size_(0),
    model_state_pos_(models.size()) {
  for (int i = 0; i < models_.size(); ++i) {
    model_state_pos_[i] = state_size_;
    state_size_ += models_[i]->NumBytesContext();
  }
}

void ModelSet::AddFeaturesToEdge(const SentenceMetadata& smeta,
                                 const Hypergraph& /* hg */,
                                 const vector<string>& node_states,
                                 Hypergraph::Edge* edge,
                                 string* context,
                                 prob_t* combination_cost_estimate) const {
  context->resize(state_size_);
  memset(&(*context)[0], 0, state_size_); //FIXME: only context.data() is required to be contiguous, and it become sinvalid after next string operation.  use SmallVector<char>?  ValueArray? (higher performance perhaps, fixed size)
  SparseVector<double> est_vals;  // only computed if combination_cost_estimate is non-NULL
  if (combination_cost_estimate) *combination_cost_estimate = prob_t::One();
  for (int i = 0; i < models_.size(); ++i) {
    const FeatureFunction& ff = *models_[i];
    void* cur_ff_context = NULL;
    vector<const void*> ants(edge->tail_nodes_.size());
    bool has_context = ff.NumBytesContext() > 0;
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

void ModelSet::AddFinalFeatures(const std::string& state, Hypergraph::Edge* edge,SentenceMetadata const& smeta) const {
  assert(1 == edge->rule_->Arity());

  for (int i = 0; i < models_.size(); ++i) {
    const FeatureFunction& ff = *models_[i];
    const void* ant_state = NULL;
    bool has_context = ff.NumBytesContext() > 0;
    if (has_context) {
      int spos = model_state_pos_[i];
      ant_state = &state[spos];
    }
    ff.FinalTraversalFeatures(smeta, *edge, ant_state, &edge->feature_values_);
  }
  edge->edge_prob_.logeq(edge->feature_values_.dot(weights_));
}

