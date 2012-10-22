#include "ff.h"
#include <iostream>
#include <sstream>

#include "hg.h"

using namespace std;

// example of a "stateful" feature made available as an external library
// This feature looks nodes and their daughters and fires an indicator based
// on the arities of the rules involved.
// (X (X a) b (X c)) - this is a 2 arity parent with children of 0 and 0 arity
//                     so you get MAF_2_0_0=1
class ParentChildrenArityFeatures : public FeatureFunction {
 public:
  ParentChildrenArityFeatures(const string& param) : fids(16, vector<int>(256, -1)) {
    SetStateSize(1); // number of bytes extra state required by this Feature
  }
  virtual void FinalTraversalFeatures(const void* context,
                                      SparseVector<double>* features) const {
    // Goal always is arity 1, so there's no discriminative value of
    // computing a feature
  }
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     FeatureVector* features,
                                     FeatureVector* estimated_features,
                                     void* context) const {
    unsigned child_arity_code = 0;
    for (unsigned j = 0; j < ant_contexts.size(); ++j) {
      child_arity_code <<= 4;
      child_arity_code |= *reinterpret_cast<const unsigned char*>(ant_contexts[j]);
    }
    int& fid = fids[edge.Arity()][child_arity_code]; // reference!
    if (fid < 0) {
      ostringstream feature_string;
      feature_string << "MAF_" << edge.Arity();
      for (unsigned j = 0; j < ant_contexts.size(); ++j)
        feature_string << '_' << 
          static_cast<int>(*reinterpret_cast<const unsigned char*>(ant_contexts[j]));
      fid = FD::Convert(feature_string.str());
    }
    features->set_value(fid, 1.0);
    *reinterpret_cast<unsigned char*>(context) = edge.Arity(); // save state
  }
 private:
  mutable vector<vector<int> > fids;
};

// IMPORTANT: this function must be implemented by any external FF library
// if your library has multiple features, you can use str to configure things
extern "C" FeatureFunction* create_ff(const string& str) {
  return new ParentChildrenArityFeatures(str);
}


