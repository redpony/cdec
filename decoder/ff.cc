#include "ff.h"

#include "tdict.h"
#include "hg.h"

using namespace std;

FeatureFunction::~FeatureFunction() {}

void FeatureFunction::PrepareForInput(const SentenceMetadata&) {}

void FeatureFunction::FinalTraversalFeatures(const void* /* ant_state */,
                                             SparseVector<double>* /* features */) const {}

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

void FeatureFunction::TraversalFeaturesImpl(const SentenceMetadata&,
                                        const Hypergraph::Edge&,
                                        const std::vector<const void*>&,
                                        SparseVector<double>*,
                                        SparseVector<double>*,
                                        void*) const {
  cerr << "TraversalFeaturesImpl not implemented - override it or TraversalFeaturesLog\n";
  abort();
}


