#ifndef _TROMBLE_LOSS_H_
#define _TROMBLE_LOSS_H_

#include <vector>
#include <boost/scoped_ptr.hpp>
#include <boost/utility/base_from_member.hpp>

#include "ff.h"
#include "wordid.h"

// this may not be the most elegant way to implement this computation, but since we
// may need cube pruning and state splitting, we reuse the feature detector framework.
// the loss is then stored in a feature #0 (which is guaranteed to have weight 0 and
// never be a "real" feature).
class TrombleLossComputerImpl;
class TrombleLossComputer : private boost::base_from_member<boost::scoped_ptr<TrombleLossComputerImpl> >, public FeatureFunction {
 private:
  typedef boost::scoped_ptr<TrombleLossComputerImpl> PImpl;
  typedef FeatureFunction Base;

 public:
  // String parameters are ref.txt num_ref weight1 weight2 ... weightn
  // where ref.txt contains references on per line, with num_ref references per sentence
  // The weights are the weight on each length n-gram.
  explicit TrombleLossComputer(const std::string &params);

  ~TrombleLossComputer();

 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* out_context) const;
 private:
  const int fid_;
};

#endif
