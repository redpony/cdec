#ifndef FEATURE_ACCUM_H
#define FEATURE_ACCUM_H

#include "ff.h"
#include "sparse_vector.h"
#include "value_array.h"

struct SparseFeatureAccumulator : public FeatureVector {
  typedef FeatureVector State;
  SparseFeatureAccumulator() { assert(!"this code is disabled");  }
  template <class FF>
  FeatureVector const& describe(FF const& ) { return *this; }
  void Store(FeatureVector *fv) const {
//NO    fv->set_from(*this);
  }
  template <class FF>
  void Store(FF const& /* ff */,FeatureVector *fv) const {
//NO    fv->set_from(*this);
  }
  template <class FF>
  void Add(FF const& /* ff */,FeatureVector const& fv) {
    (*this)+=fv;
  }
  void Add(FeatureVector const& fv) {
    (*this)+=fv;
  }
  /*
  SparseFeatureAccumulator(FeatureVector const& fv) : State(fv) {}
  FeatureAccumulator(Features const& fids) {}
  FeatureAccumulator(Features const& fids,FeatureVector const& fv) : State(fv) {}
  void Add(Features const& fids,FeatureVector const& fv) {
    *this += fv;
  }
  */
  void Add(int i,Featval v) {
//NO    (*this)[i]+=v;
  }
  void Add(Features const& fids,int i,Featval v) {
//NO    (*this)[i]+=v;
  }
};

struct SingleFeatureAccumulator {
  typedef Featval State;
  typedef SingleFeatureAccumulator Self;
  State v;
  /*
  void operator +=(State const& o) {
    v+=o;
  }
  */
  void operator +=(Self const& s) {
    v+=s.v;
  }
  SingleFeatureAccumulator() : v() {}
  template <class FF>
  State const& describe(FF const& ) const { return v; }

  template <class FF>
  void Store(FF const& ff,FeatureVector *fv) const {
    fv->set_value(ff.fid_,v);
  }
  void Store(Features const& fids,FeatureVector *fv) const {
    assert(fids.size()==1);
    fv->set_value(fids[0],v);
  }
  /*
  SingleFeatureAccumulator(Features const& fids) { assert(fids.size()==1); }
  SingleFeatureAccumulator(Features const& fids,FeatureVector const& fv)
  {
    assert(fids.size()==1);
    v=fv.get_singleton();
  }
  */

  template <class FF>
  void Add(FF const& ff,FeatureVector const& fv) {
    v+=fv.get(ff.fid_);
  }
  void Add(FeatureVector const& fv) {
    v+=fv.get_singleton();
  }

  void Add(Features const& fids,FeatureVector const& fv) {
    v += fv.get(fids[0]);
  }
  void Add(Featval dv) {
    v+=dv;
  }
  void Add(int,Featval dv) {
    v+=dv;
  }
  void Add(FeatureVector const& fids,int i,Featval dv) {
    assert(fids.size()==1 && i==0);
    v+=dv;
  }
};


#if 0
// omitting this so we can default construct an accum.  might be worth resurrecting in the future
struct ArrayFeatureAccumulator : public ValueArray<Featval> {
  typedef ValueArray<Featval> State;
  template <class Fsa>
  ArrayFeatureAccumulator(Fsa const& fsa) : State(fsa.features_.size()) { }
  ArrayFeatureAccumulator(Features const& fids) : State(fids.size()) {  }
  ArrayFeatureAccumulator(Features const& fids) : State(fids.size()) {  }
  ArrayFeatureAccumulator(Features const& fids,FeatureVector const& fv) : State(fids.size()) {
    for (int i=0,e=i<fids.size();i<e;++i)
      (*this)[i]=fv.get(i);
  }
  State const& describe(Features const& fids) const { return *this; }
  void Store(Features const& fids,FeatureVector *fv) const {
    assert(fids.size()==size());
    for (int i=0,e=i<fids.size();i<e;++i)
      fv->set_value(fids[i],(*this)[i]);
  }
  void Add(Features const& fids,FeatureVector const& fv) {
    for (int i=0,e=i<fids.size();i<e;++i)
      (*this)[i]+=fv.get(i);
  }
  void Add(FeatureVector const& fids,int i,Featval v) {
    (*this)[i]+=v;
  }
};
#endif


#endif
