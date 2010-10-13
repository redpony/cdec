#ifndef _FF_WORD_ALIGN_H_
#define _FF_WORD_ALIGN_H_

#include "ff.h"
#include "array2d.h"

#include <boost/multi_array.hpp>

class RelativeSentencePosition : public FeatureFunction {
 public:
  RelativeSentencePosition(const std::string& param);
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
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

class Model2BinaryFeatures : public FeatureFunction {
 public:
  Model2BinaryFeatures(const std::string& param);
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* out_context) const;
 private:
  boost::multi_array<int, 3> fids_;
};

class MarkovJump : public FeatureFunction {
 public:
  MarkovJump(const std::string& param);
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* out_context) const;
 private:
  const int fid_;
  bool binary_params_;
  std::vector<std::map<int, int> > flen2jump2fid_;
};

class MarkovJumpFClass : public FeatureFunction {
 public:
  MarkovJumpFClass(const std::string& param);
  virtual void FinalTraversalFeatures(const void* context,
                                      SparseVector<double>* features) const;
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const;

  void FireFeature(const SentenceMetadata& smeta,
                   int prev_src_pos,
                   int cur_src_pos,
                   SparseVector<double>* features) const;

 private:
  std::vector<std::map<WordID, std::map<int, int> > > fids_;  // flen -> fclass -> jumpsize -> fid
  std::vector<std::vector<WordID> > pos_;
};

typedef std::map<WordID, int> Class2FID;
typedef std::map<WordID, Class2FID> Class2Class2FID;
typedef std::map<WordID, Class2Class2FID> Class2Class2Class2FID;
class SourceBigram : public FeatureFunction {
 public:
  SourceBigram(const std::string& param);
  virtual void FinalTraversalFeatures(const void* context,
                                      SparseVector<double>* features) const;
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const;
 private:
  void FireFeature(WordID src,
                   WordID trg,
                   SparseVector<double>* features) const;
  mutable Class2Class2FID fmap_;
  mutable Class2FID ufmap_;
};

class SourcePOSBigram : public FeatureFunction {
 public:
  SourcePOSBigram(const std::string& param);
  virtual void FinalTraversalFeatures(const void* context,
                                      SparseVector<double>* features) const;
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const;
 private:
  void FireFeature(WordID src,
                   WordID trg,
                   SparseVector<double>* features) const;
  mutable Class2Class2FID fmap_;
  std::vector<std::vector<WordID> > pos_;
};

class LexicalTranslationTrigger : public FeatureFunction {
 public:
  LexicalTranslationTrigger(const std::string& param);
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
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

class AlignerResults : public FeatureFunction {
 public:
  AlignerResults(const std::string& param);
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* out_context) const;
 private:
  int fid_;
  std::vector<boost::shared_ptr<Array2D<bool> > > is_aligned_;
  mutable int cur_sent_;
  const Array2D<bool> mutable* cur_grid_;
};

#include <tr1/unordered_map>
#include <boost/functional/hash.hpp>
#include <cassert>
class BlunsomSynchronousParseHack : public FeatureFunction {
 public:
  BlunsomSynchronousParseHack(const std::string& param);
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* out_context) const;
 private:
  inline bool DoesNotBelong(const void* state) const {
    for (int i = 0; i < NumBytesContext(); ++i) {
      if (*(static_cast<const unsigned char*>(state) + i)) return false;
    }
    return true;
  }

  inline void AppendAntecedentString(const void* state, std::vector<WordID>* yield) const {
    int i = 0;
    int ind = 0;
    while (i < NumBytesContext() && !(*(static_cast<const unsigned char*>(state) + i))) { ++i; ind += 8; }
    // std::cerr << i << " " << NumBytesContext() << std::endl;
    assert(i != NumBytesContext());
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
    assert((end / 8) < NumBytesContext());
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
  typedef std::tr1::unordered_map<std::vector<WordID>, int, boost::hash<std::vector<WordID> > > Vec2Int;
  mutable Vec2Int cur_map_;
  const std::vector<WordID> mutable * cur_ref_;
  mutable std::vector<std::vector<WordID> > refs_;
};

#endif
