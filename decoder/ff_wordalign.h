#ifndef _FF_WORD_ALIGN_H_
#define _FF_WORD_ALIGN_H_

#include "ff.h"
#include "array2d.h"
#include "factored_lexicon_helper.h"

#include <boost/functional/hash.hpp>
#include <cassert>
#include <boost/scoped_ptr.hpp>
#include <boost/multi_array.hpp>
#ifndef HAVE_OLD_CPP
# include <unordered_map>
#else
# include <tr1/unordered_map>
namespace std { using std::tr1::unordered_map; }
#endif

class RelativeSentencePosition : public FeatureFunction {
 public:
  RelativeSentencePosition(const std::string& param);
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const HG::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* out_context) const;
 private:
  const int fid_;
  bool condition_on_fclass_;
  std::vector<std::vector<WordID> > pos_;
  std::map<WordID, int> fids_;  // fclass -> fid
};

typedef std::map<WordID, int> Class2FID;
typedef std::map<WordID, Class2FID> Class2Class2FID;
typedef std::map<WordID, Class2Class2FID> Class2Class2Class2FID;
class SourceBigram : public FeatureFunction {
 public:
  SourceBigram(const std::string& param);
  virtual void FinalTraversalFeatures(const void* context,
                                      SparseVector<double>* features) const;
  void PrepareForInput(const SentenceMetadata& smeta);
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const HG::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const;
 private:
  void FireFeature(WordID src,
                   WordID trg,
                   SparseVector<double>* features) const;
  std::string fid_str_;
  mutable Class2Class2FID fmap_;
  boost::scoped_ptr<FactoredLexiconHelper> lexmap_; // different view (stemmed, etc) of source
};

class LexNullJump : public FeatureFunction {
 public:
  LexNullJump(const std::string& param);
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const HG::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* out_context) const;
 private:
  const int fid_lex_null_;
  const int fid_null_lex_;
  const int fid_null_null_;
  const int fid_lex_lex_;
};

class NewJump : public FeatureFunction {
 public:
  NewJump(const std::string& param);
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const HG::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* out_context) const;
 private:
  void FireFeature(const SentenceMetadata& smeta,
                   const int prev_src_index,
                   const int cur_src_index,
                   SparseVector<double>* features) const;

  WordID GetSourceWord(int sentence_id, int index) const {
    if (index < 0) return kBOS_;
    assert(src_.size() > sentence_id);
    const std::vector<WordID>& v = src_[sentence_id];
    if (index >= v.size()) return kEOS_;
    return v[index];
  }

  const WordID kBOS_;
  const WordID kEOS_;
  bool use_binned_log_lengths_;
  bool flen_;
  bool elen_;
  bool f0_;
  bool fm1_;
  bool fp1_;
  bool fprev_;
  std::vector<std::vector<WordID> > src_;
  std::string fid_str_;  // identifies configuration uniquely
};

class LexicalTranslationTrigger : public FeatureFunction {
 public:
  LexicalTranslationTrigger(const std::string& param);
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const HG::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const;
 private:
  void FireFeature(WordID trigger,
                   WordID src,
                   WordID trg,
                   SparseVector<double>* features) const;
  mutable Class2Class2Class2FID fmap_;  // trigger,src,trg
  mutable Class2Class2FID target_fmap_;  // trigger,src,trg
  std::vector<std::vector<WordID> > triggers_;
};

class BlunsomSynchronousParseHack : public FeatureFunction {
 public:
  BlunsomSynchronousParseHack(const std::string& param);
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const HG::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* out_context) const;
 private:
  inline bool DoesNotBelong(const void* state) const {
    for (int i = 0; i < StateSize(); ++i) {
      if (*(static_cast<const unsigned char*>(state) + i)) return false;
    }
    return true;
  }

  inline void AppendAntecedentString(const void* state, std::vector<WordID>* yield) const {
    int i = 0;
    int ind = 0;
    while (i < StateSize() && !(*(static_cast<const unsigned char*>(state) + i))) { ++i; ind += 8; }
    // std::cerr << i << " " << StateSize() << std::endl;
    assert(i != StateSize());
    assert(ind < cur_ref_->size());
    int cur = *(static_cast<const unsigned char*>(state) + i);
    int comp = 1;
    while (comp < 256 && (comp & cur) == 0) { comp <<= 1; ++ind; }
    assert(ind < cur_ref_->size());
    assert(comp < 256);
    do {
      assert(ind < cur_ref_->size());
      yield->push_back((*cur_ref_)[ind]);
      ++ind;
      comp <<= 1;
      if (comp == 256) {
        comp = 1;
        ++i;
        cur = *(static_cast<const unsigned char*>(state) + i);
      }
    } while (comp & cur);
  }

  inline void SetStateMask(int start, int end, void* state) const {
    assert((end / 8) < StateSize());
    int i = 0;
    int comp = 1;
    for (int j = 0; j < start; ++j) {
      comp <<= 1;
      if (comp == 256) {
        ++i;
        comp = 1;
      }
    }
    //std::cerr << "SM: " << i << "\n";
    for (int j = start; j < end; ++j) {
      *(static_cast<unsigned char*>(state) + i) |= comp;
      //std::cerr << "  " << comp << "\n";
      comp <<= 1;
      if (comp == 256) {
        ++i;
        comp = 1;
      }
    }
    //std::cerr << "   MASK: " << ((int)*(static_cast<unsigned char*>(state))) << "\n";
  }

  const int fid_;
  mutable int cur_sent_;
  typedef std::unordered_map<std::vector<WordID>, int, boost::hash<std::vector<WordID> > > Vec2Int;
  mutable Vec2Int cur_map_;
  const std::vector<WordID> mutable * cur_ref_;
  mutable std::vector<std::vector<WordID> > refs_;
};

// association feature type look up a pair (e,f) in a table and return a vector
// of feature values
class WordPairFeatures : public FeatureFunction {
 public:
  WordPairFeatures(const std::string& param);
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const HG::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const;

 private:
  std::vector<WordID> fkeys_;  // parallel to values_
  std::vector<std::map<WordID, SparseVector<float> > > values_;  // fkeys_index -> e -> value
};

// fires when a len(word) >= length_min_ is translated as itself and then a self-transition is made
class IdentityCycleDetector : public FeatureFunction {
 public:
  IdentityCycleDetector(const std::string& param);
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const HG::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const;
 private:
  int length_min_;
  int fid_;
  mutable std::map<WordID, bool> big_enough_;
};

class InputIndicator : public FeatureFunction {
 public:
  InputIndicator(const std::string& param);
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const HG::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const;
 private:
  void FireFeature(WordID src,
                   SparseVector<double>* features) const;
  mutable Class2FID fmap_;
};

class Fertility : public FeatureFunction {
 public:
  Fertility(const std::string& param);
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const HG::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const;
 private:
  mutable std::map<WordID, int> fids_;
};

#endif
