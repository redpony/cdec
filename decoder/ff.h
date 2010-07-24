#ifndef _FF_H_
#define _FF_H_

#include <vector>
#include "fdict.h"
#include "hg.h"

class SentenceMetadata;
class FeatureFunction;  // see definition below

typedef std::vector<WordID> Features; // set of features ids

// if you want to develop a new feature, inherit from this class and
// override TraversalFeaturesImpl(...).  If it's a feature that returns /
// depends on context, you may also need to implement
// FinalTraversalFeatures(...)
class FeatureFunction {
 public:
  std::string name; // set by FF factory using usage()
  bool debug; // also set by FF factory checking param for immediate initial "debug"
  FeatureFunction() : state_size_() {}
  explicit FeatureFunction(int state_size) : state_size_(state_size) {}
  virtual ~FeatureFunction();

  // override this.  not virtual because we want to expose this to factory template for help before creating a FF
  static std::string usage(bool show_params,bool show_details) {
    return usage_helper("FIXME_feature_needs_name","[no parameters]","[no documentation yet]",show_params,show_details);
  }
  static std::string usage_helper(std::string const& name,std::string const& params,std::string const& details,bool show_params,bool show_details);
  static Features single_feature(int feat);
public:
  // stateless feature that doesn't depend on source span: override and return true.  then your feature can be precomputed over rules.
  virtual bool rule_feature() const { return false; }

  //OVERRIDE THIS:
  virtual Features features() const { return Features(); }
  // returns the number of bytes of context that this feature function will
  // (maximally) use.  By default, 0 ("stateless" models in Hiero/Joshua).
  // NOTE: this value is fixed for the instance of your class, you cannot
  // use different amounts of memory for different nodes in the forest.
  inline int NumBytesContext() const { return state_size_; }

  // Compute the feature values and (if this applies) the estimates of the
  // feature values when this edge is used incorporated into a larger context
  inline void TraversalFeatures(const SentenceMetadata& smeta,
                                const Hypergraph::Edge& edge,
                                const std::vector<const void*>& ant_contexts,
                                FeatureVector* features,
                                FeatureVector* estimated_features,
                                void* out_state) const {
    TraversalFeaturesImpl(smeta, edge, ant_contexts,
                          features, estimated_features, out_state);
    // TODO it's easy for careless feature function developers to overwrite
    // the end of their state and clobber someone else's memory.  These bugs
    // will be horrendously painful to track down.  There should be some
    // optional strict mode that's enforced here that adds some kind of
    // barrier between the blocks reserved for the residual contexts
  }

  // if there's some state left when you transition to the goal state, score
  // it here.  For example, the language model computes the cost of adding
  // <s> and </s>.
protected:
  virtual void FinalTraversalFeatures(const void* residual_state,
                                      FeatureVector* final_features) const;
public:
  //override either this or one of above.
  virtual void FinalTraversalFeatures(const SentenceMetadata& /* smeta */,
                                      const Hypergraph::Edge& /* edge */,
                                      const void* residual_state,
                                      FeatureVector* final_features) const {
    FinalTraversalFeatures(residual_state,final_features);
  }


 protected:
  // context is a pointer to a buffer of size NumBytesContext() that the
  // feature function can write its state to.  It's up to the feature function
  // to determine how much space it needs and to determine how to encode its
  // residual contextual information since it is OPAQUE to all clients outside
  // of the particular FeatureFunction class.  There is one exception:
  // equality of the contents (i.e., memcmp) is required to determine whether
  // two states can be combined.
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     FeatureVector* features,
                                     FeatureVector* estimated_features,
                                     void* context) const = 0;

  // !!! ONLY call this from subclass *CONSTRUCTORS* !!!
  void SetStateSize(size_t state_size) {
    state_size_ = state_size;
  }
  int StateSize() const { return state_size_; }
 private:
  int state_size_;
};


// word penalty feature, for each word on the E side of a rule,
// add value_
class WordPenalty : public FeatureFunction {
 public:
  Features features() const;
  WordPenalty(const std::string& param);
  static std::string usage(bool p,bool d) {
    return usage_helper("WordPenalty","","number of target words (local feature)",p,d);
  }
  bool rule_feature() const { return true; }
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     FeatureVector* features,
                                     FeatureVector* estimated_features,
                                     void* context) const;
 private:
  const int fid_;
  const double value_;
};

class SourceWordPenalty : public FeatureFunction {
 public:
  bool rule_feature() const { return true; }
  Features features() const;
  SourceWordPenalty(const std::string& param);
  static std::string usage(bool p,bool d) {
    return usage_helper("SourceWordPenalty","","number of source words (local feature, and meaningless except when input has non-constant number of source words, e.g. segmentation/morphology/speech recognition lattice)",p,d);
  }
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     FeatureVector* features,
                                     FeatureVector* estimated_features,
                                     void* context) const;
 private:
  const int fid_;
  const double value_;
};

#define DEFAULT_MAX_ARITY 9
#define DEFAULT_MAX_ARITY_STRINGIZE(x) #x
#define DEFAULT_MAX_ARITY_STRINGIZE_EVAL(x) DEFAULT_MAX_ARITY_STRINGIZE(x)
#define DEFAULT_MAX_ARITY_STR DEFAULT_MAX_ARITY_STRINGIZE_EVAL(DEFAULT_MAX_ARITY)

class ArityPenalty : public FeatureFunction {
 public:
  bool rule_feature() const { return true; }
  Features features() const;
  ArityPenalty(const std::string& param);
  static std::string usage(bool p,bool d) {
    return usage_helper("ArityPenalty","[MaxArity(default " DEFAULT_MAX_ARITY_STR ")]","Indicator feature Arity_N=1 for rule of arity N (local feature).  0<=N<=MaxArity(default " DEFAULT_MAX_ARITY_STR ")",p,d);
  }

 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     FeatureVector* features,
                                     FeatureVector* estimated_features,
                                     void* context) const;
 private:
  std::vector<WordID> fids_;
  const double value_;
};

// this class is a set of FeatureFunctions that can be used to score, rescore,
// etc. a (translation?) forest
class ModelSet {
 public:
  ModelSet() : state_size_(0) {}

  ModelSet(const std::vector<double>& weights,
           const std::vector<const FeatureFunction*>& models);

  // sets edge->feature_values_ and edge->edge_prob_
  // NOTE: edge must not necessarily be in hg.edges_ but its TAIL nodes
  // must be.  edge features are supposed to be overwritten, not added to (possibly because rule features aren't in ModelSet so need to be left alone
  void AddFeaturesToEdge(const SentenceMetadata& smeta,
                         const Hypergraph& hg,
                         const std::vector<std::string>& node_states,
                         Hypergraph::Edge* edge,
                         std::string* residual_context,
                         prob_t* combination_cost_estimate = NULL) const;

  //this is called INSTEAD of above when result of edge is goal (must be a unary rule - i.e. one variable, but typically it's assumed that there are no target terminals either (e.g. for LM))
  void AddFinalFeatures(const std::string& residual_context,
                        Hypergraph::Edge* edge,
                        SentenceMetadata const& smeta) const;

  bool empty() const { return models_.empty(); }

  bool stateless() const { return !state_size_; }
  Features all_features(std::ostream *warnings=0,bool warn_fid_zero=false); // this will warn about duplicate features as well (one function overwrites the feature of another).  also resizes weights_ so it is large enough to hold the (0) weight for the largest reported feature id.  since 0 is a NULL feature id, it's never included.  if warn_fid_zero, then even the first 0 id is
  void show_features(std::ostream &out,std::ostream &warn,bool warn_zero_wt=true); //show features and weights
 private:
  std::vector<const FeatureFunction*> models_;
  std::vector<double> weights_;
  int state_size_;
  std::vector<int> model_state_pos_;
};

#endif
