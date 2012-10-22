#ifndef _FF_RULESHAPE_H_
#define _FF_RULESHAPE_H_

#include <vector>
#include "ff.h"

class RuleShapeFeatures : public FeatureFunction {
 public:
  RuleShapeFeatures(const std::string& param);
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const HG::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const;
 private:
  struct Node {
    int fid_;
    Node() : fid_(-1) {}
    std::map<bool, Node> next_;
  };
  Node fidtree_;
  static const Node* Advance(const Node* cur, bool val) {
    std::map<bool, Node>::const_iterator it = cur->next_.find(val);
    if (it == cur->next_.end()) return NULL;
    return &it->second;
  }
};

#endif
