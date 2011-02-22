#ifndef _FF_SPANS_H_
#define _FF_SPANS_H_

#include <vector>
#include <map>
#include "ff.h"
#include "array2d.h"
#include "wordid.h"

class SpanFeatures : public FeatureFunction {
 public:
  SpanFeatures(const std::string& param);
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const;
  virtual void PrepareForInput(const SentenceMetadata& smeta);
 private:
  WordID MapIfNecessary(const WordID& w) const;
  const int kS;
  const int kX;
  Array2D<std::pair<int,int> > span_feats_; // first for X, second for S
  std::vector<int> end_span_ids_;
  std::vector<int> beg_span_ids_;
  std::map<WordID, WordID> word2class_;  // optional projection to coarser class

  // collapsed feature values
  bool use_collapsed_features_;
  int fid_beg_;
  int fid_end_;
  int fid_span_s_;
  int fid_span_;
  std::map<std::string, double> feat2val_;
  std::vector<double> end_span_vals_;
  std::vector<double> beg_span_vals_;
  Array2D<std::pair<double,double> > span_vals_;

  WordID oov_;
};

#endif
