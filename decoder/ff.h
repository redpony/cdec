#ifndef _FF_H_
#define _FF_H_

#define DEBUG_INIT 0
#if DEBUG_INIT
# include <iostream>
# define DBGINIT(a) do { std::cerr<<a<<"\n"; } while(0)
#else
# define DBGINIT(a)
#endif

#include <stdint.h>
#include <vector>
#include <cstring>
#include "fdict.h"
#include "hg.h"
#include "feature_vector.h"
#include "value_array.h"

class SentenceMetadata;
class FeatureFunction;  // see definition below

typedef std::vector<WordID> Features; // set of features ids

// if you want to develop a new feature, inherit from this class and
// override TraversalFeaturesImpl(...).  If it's a feature that returns /
// depends on context, you may also need to implement
// FinalTraversalFeatures(...)
class FeatureFunction {
 public:
  std::string name_; // set by FF factory using usage()
  bool debug_; // also set by FF factory checking param for immediate initial "debug"
  //called after constructor, but before name_ and debug_ have been set
  virtual void Init() { DBGINIT("default FF::Init name="<<name_); }
  virtual void init_name_debug(std::string const& n,bool debug) {
    name_=n;
    debug_=debug;
  }
  bool debug() const { return debug_; }
  FeatureFunction() : state_size_() {}
  explicit FeatureFunction(int state_size) : state_size_(state_size) {}
  virtual ~FeatureFunction();
  bool IsStateful() const { return state_size_ > 0; }

  // override this.  not virtual because we want to expose this to factory template for help before creating a FF
  static std::string usage(bool show_params,bool show_details) {
    return usage_helper("FIXME_feature_needs_name","[no parameters]","[no documentation yet]",show_params,show_details);
  }
  static std::string usage_helper(std::string const& name,std::string const& params,std::string const& details,bool show_params,bool show_details);
  static Features single_feature(int feat);
public:

  // stateless feature that doesn't depend on source span: override and return true.  then your feature can be precomputed over rules.
  virtual bool rule_feature() const { return false; }

  // called once, per input, before any feature calls to TraversalFeatures, etc.
  // used to initialize sentence-specific data structures
  virtual void PrepareForInput(const SentenceMetadata& smeta);

  //OVERRIDE THIS:
  virtual Features features() const { return single_feature(FD::Convert(name_)); }
  // returns the number of bytes of context that this feature function will
  // (maximally) use.  By default, 0 ("stateless" models in Hiero/Joshua).
  // NOTE: this value is fixed for the instance of your class, you cannot
  // use different amounts of memory for different nodes in the forest.  this will be read as soon as you create a ModelSet, then fixed forever on
  inline int NumBytesContext() const { return state_size_; }

  // Compute the feature values and (if this applies) the estimates of the
  // feature values when this edge is used incorporated into a larger context
  inline void TraversalFeatures(const SentenceMetadata& smeta,
                                Hypergraph::Edge& edge,
                                const std::vector<const void*>& ant_contexts,
                                FeatureVector* features,
                                FeatureVector* estimated_features,
                                void* out_state) const {
    TraversalFeaturesLog(smeta, edge, ant_contexts,
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
                                      Hypergraph::Edge& /* edge */, // so you can log()
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

  // by Log, I mean that the edge is non-const only so you can log to it with INFO_EDGE(edge,msg<<"etc.").  most features don't use this so implement the below.  it has a different name to allow a default implementation without name hiding when inheriting + overriding just 1.
  virtual void TraversalFeaturesLog(const SentenceMetadata& smeta,
                                    Hypergraph::Edge& edge, // this is writable only so you can use log()
                                     const std::vector<const void*>& ant_contexts,
                                     FeatureVector* features,
                                     FeatureVector* estimated_features,
                                     void* context) const {
    TraversalFeaturesImpl(smeta,edge,ant_contexts,features,estimated_features,context);
  }

  // override above or below.
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     Hypergraph::Edge const& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     FeatureVector* features,
                                     FeatureVector* estimated_features,
                                     void* context) const;

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

void show_features(Features const& features,DenseWeightVector const& weights,std::ostream &out,std::ostream &warn,bool warn_zero_wt=true); //show features and weights

template <class FFp>
Features all_features(std::vector<FFp> const& models_,DenseWeightVector &weights_,std::ostream *warn=0,bool warn_fid_0=false) {
  using namespace std;
  Features ffs;
#define WARNFF(x) do { if (warn) { *warn << "WARNING: "<< x << endl; } } while(0)
  typedef map<WordID,string> FFM;
  FFM ff_from;
  for (unsigned i=0;i<models_.size();++i) {
    string const& ffname=models_[i]->name_;
    Features si=models_[i]->features();
    if (si.empty()) {
      WARNFF(ffname<<" doesn't yet report any feature IDs - either supply feature weight, or use --no_freeze_feature_set, or implement features() method");
    }
    unsigned n0=0;
    for (unsigned j=0;j<si.size();++j) {
      WordID fid=si[j];
      if (!fid) ++n0;
      if (fid >= weights_.size())
        weights_.resize(fid+1);
      if (warn_fid_0 || fid) {
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

template <class FFp>
void show_all_features(std::vector<FFp> const& models_,DenseWeightVector &weights_,std::ostream &out,std::ostream &warn,bool warn_fid_0=true,bool warn_zero_wt=true) {
  return show_features(all_features(models_,weights_,&warn,warn_fid_0),weights_,out,warn,warn_zero_wt);
}

typedef ValueArray<uint8_t> FFState; // this is about 10% faster than string.
//typedef std::string FFState;

//FIXME: only context.data() is required to be contiguous, and it becomes invalid after next string operation.  use ValueArray instead? (higher performance perhaps, save a word due to fixed size)
typedef std::vector<FFState> FFStates;

// this class is a set of FeatureFunctions that can be used to score, rescore,
// etc. a (translation?) forest
class ModelSet {
 public:
  ModelSet(const std::vector<double>& weights,
           const std::vector<const FeatureFunction*>& models);

  // sets edge->feature_values_ and edge->edge_prob_
  // NOTE: edge must not necessarily be in hg.edges_ but its TAIL nodes
  // must be.  edge features are supposed to be overwritten, not added to (possibly because rule features aren't in ModelSet so need to be left alone
  void AddFeaturesToEdge(const SentenceMetadata& smeta,
                         const Hypergraph& hg,
                         const FFStates& node_states,
                         Hypergraph::Edge* edge,
                         FFState* residual_context,
                         prob_t* combination_cost_estimate = NULL) const;

  //this is called INSTEAD of above when result of edge is goal (must be a unary rule - i.e. one variable, but typically it's assumed that there are no target terminals either (e.g. for LM))
  void AddFinalFeatures(const FFState& residual_context,
                        Hypergraph::Edge* edge,
                        SentenceMetadata const& smeta) const;

  // this is called once before any feature functions apply to a hypergraph
  // it can be used to initialize sentence-specific data structures
  void PrepareForInput(const SentenceMetadata& smeta);

  bool empty() const { return models_.empty(); }

  bool stateless() const { return !state_size_; }
  Features all_features(std::ostream *warnings=0,bool warn_fid_zero=false); // this will warn about duplicate features as well (one function overwrites the feature of another).  also resizes weights_ so it is large enough to hold the (0) weight for the largest reported feature id.  since 0 is a NULL feature id, it's never included.  if warn_fid_zero, then even the first 0 id is
  void show_features(std::ostream &out,std::ostream &warn,bool warn_zero_wt=true);

 private:
  std::vector<const FeatureFunction*> models_;
  const std::vector<double>& weights_;
  int state_size_;
  std::vector<int> model_state_pos_;
};

#endif
