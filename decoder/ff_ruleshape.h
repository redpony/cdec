#ifndef FF_RULESHAPE_H_
#define FF_RULESHAPE_H_

#include <vector>
#include <map>
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

class RuleShapeFeatures2 : public FeatureFunction {
 public:
  ~RuleShapeFeatures2();
  RuleShapeFeatures2(const std::string& param);
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
    Node() : fid_() {}
    std::map<WordID, Node> next_;
  };
  mutable Node fidtree_;

  inline WordID MapE(WordID w) const {
    if (w <= 0) return kNT;
    unsigned res = 0;
    if (w < e2class_.size()) res = e2class_[w];
    if (!res) res = kUNK;
    return res;
  }

  inline WordID MapF(WordID w) const {
    if (w <= 0) return kNT;
    unsigned res = 0;
    if (w < f2class_.size()) res = f2class_[w];
    if (!res) res = kUNK;
    return res;
  }

  // prfx_size=0 => use full word classes otherwise truncate to specified length
  void LoadWordClasses(const std::string& fname, unsigned pfxsize, std::vector<WordID>* pv);
  const WordID kNT;
  const WordID kUNK;
  std::vector<WordID> e2class_;
  std::vector<WordID> f2class_;
  bool has_src_;
  bool has_trg_;
};

#endif
