#ifndef FF_FSA_H
#define FF_FSA_H

//SEE ALSO: ff_fsa_dynamic.h, ff_from_fsa.h

#define FSA_DEBUG
#ifdef FSA_DEBUG
# include <iostream>
# define FSADBG(x) do { std::cerr << x; } while(0)
#else
# define FSADBG(x)
#endif

#include <stdint.h> //C99
#include <string>
#include "ff.h"
#include "sparse_vector.h"
#include "value_array.h" // used to hold state
#include "tdict.h"
#include "hg.h"
#include "sentences.h"

typedef ValueArray<uint8_t> Bytes;

/*
  features whose score is just some PFSA over target string.  TODO: could decide to give access to source span of scanned words as well if someone devises a feature that can use it

  state is some fixed width byte array.  could actually be a void *, WordID sequence, whatever.

*/

// it's not necessary to inherit from this, but you probably should to save yourself some boilerplate.  defaults to no-state
struct FsaFeatureFunctionBase {
protected:
  Bytes start,h_start; // start state and estimated-features (heuristic) start state.  set these.  default empty.
  Sentence end_phrase_; // words appended for final traversal (final state cost is assessed using Scan) e.g. "</s>" for lm.
  int state_bytes_; // don't forget to set this. default 0 (it may depend on params of course)
  void set_state_bytes(int sb=0) {
    if (start.size()!=sb) start.resize(sb);
    if (h_start.size()!=sb) h_start.resize(sb);
    state_bytes_=sb;
  }
  int fid_; // you can have more than 1 feature of course.
  void init_fid(std::string const& name) { // call this, though, if you have a single feature
    fid_=FD::Convert(name);
  }
  inline void static to_state(void *state,char const* begin,char const* end) {
    std::memcpy(state,begin,end-begin);
  }
  inline void static to_state(void *state,char const* begin,int n) {
    std::memcpy(state,begin,n);
  }
  template <class T>
  inline void static to_state(void *state,T const* begin,int n) {
    to_state(state,(char const*)begin,n);
  }
  template <class T>
  inline void static to_state(void *state,T const* begin,T const* end) {
    to_state(state,(char const*)begin,(char const*)end);
  }
public:
  //edges may have old features on them.  override if you have more than 1 fid.  we need to call this explicitly because edges may have old feature values already, and I chose to use add_value (+=) to simplify scanning a phrase, rather than set_value (=) for fsa ffs.  could revisit this and use set_value and therefore sum
  void init_features(FeatureVector *fv) const {
    fv->set_value(fid_,0);
    //erase(fid_)
  }
  // return m: all strings x with the same final m+1 letters must end in this state
  /* markov chain of order m: P(xn|xn-1...x1)=P(xn|xn-1...xn-m) */
  int markov_order() const { return 0; } // override if you use state.  order 0 implies state_bytes()==0 as well, as far as scoring/splitting is concerned (you can still track state, though)
  //TODO: if we wanted, we could mark certain states as maximal-context, but this would lose our fixed amount of left context in ff_from_fsa, and lose also our vector operations (have to scan left words 1 at a time, checking always to see where you change from h to inside - BUT, could detect equivalent LM states, which would be nice).

  Features features() const { // override this if >1 fid
    return FeatureFunction::single_feature(fid_);
  }

  // override this (static)
  static std::string usage(bool param,bool verbose) {
    return FeatureFunction::usage_helper("unnamed_fsa_feature","","",param,verbose);
  }
  int state_bytes() const { return state_bytes_; } // or override this
  void const* start_state() const {
    return start.begin();
  }
  void const * heuristic_start_state() const {
    return h_start.begin();
  }
  Sentence const& end_phrase() const { return end_phrase_; }
  // move from state to next_state after seeing word x, while emitting features->add_value(fid,val) possibly with duplicates.  state and next_state will never be the same memory.
  //TODO: decide if we want to require you to support dest same as src, since that's how we use it most often in ff_from_fsa bottom-up wrapper (in l->r scoring, however, distinct copies will be the rule), and it probably wouldn't be too hard for most people to support.  however, it's good to hide the complexity here, once (see overly clever FsaScan loop that swaps src/dest addresses repeatedly to scan a sequence by effectively swapping)

  // NOTE: if you want to e.g. track statistics, cache, whatever, cast const away or use mutable members
  void Scan(SentenceMetadata const& smeta,WordID w,void const* state,void *next_state,FeatureVector *features) const {
  }

  // don't set state-bytes etc. in ctor because it may depend on parsing param string
  FsaFeatureFunctionBase(int statesz=0) : start(statesz),h_start(statesz),state_bytes_(statesz) {  }

};

// init state is in cs; overwrite cs, ns repeatedly (alternatively).  return resulting state
template <class FsaFF>
void *FsaScan(FsaFF const& ff,SentenceMetadata const& smeta,WordID const* i, WordID const* end,FeatureVector *features, void *cs,void *ns) {
  // extra code - IT'S FOR EFFICIENCY, MAN!  IT'S OK!  definitely no bugs here.
  void *os,*es;
  if ((end-i)&1) { // odd # of words
    os=cs;
    es=ns;
    goto odd;
  } else {
    i+=1;
    es=cs;
    os=ns;
  }
  for (;i<end;i+=2) {
    ff.Scan(smeta,i[-1],es,os,features); // e->o
  odd:
    ff.Scan(smeta,i[0],os,es,features); // o->e
  }
  return es;
}

// if you have a more efficient implementation for scanning a phrase than one word at a time (e.g. LM context using sliding window buffer rather than rotating through a fixed state size), you can override this
template <class FsaFF>
void Scan(FsaFF const& ff,SentenceMetadata const& smeta,WordID const* i,WordID const* e,void const* state,void *next_state,FeatureVector *features) {
  int ssz=ff.state_bytes();
  if (!ssz) {
    for (;i<e;++i)
      ff.Scan(smeta,*i,0,0,features);
    return;
  }
  Bytes tstate(ssz);
  void *tst=tstate.begin();
  bool odd=(e-i)&1;
  void *cs,*ns;
  if (odd) {
    cs=tst;
    ns=next_state;
  } else {
    cs=next_state;
    ns=tst;
  }
  std::memcpy(cs,state,ssz);
  void *est=FsaScan(ff,smeta,i,end,features,cs,ns);
  assert(est==next_state);
}


// like above Scan, but don't bother storing final state (for FinalTraversalFeatures)
template <class FF>
void AccumFeatures(FF const& ff,SentenceMetadata const& smeta,WordID const* i, WordID const* end,FeatureVector *h_features,void const* start_state) {
  int ssz=ff.state_bytes();
  if (ssz) {
    Bytes state(ssz),state2(ssz);
    void *cs=state.begin(),*ns=state2.begin();
    memcpy(cs,start_state,ff.state_bytes());
    FsaScan(ff,smeta,i,end,h_features,cs,ns);
  } else
    for (;i<end;++i)
      ff.Scan(smeta,*i,0,0,h_features);
}

// if State is pod.  sets state size and allocs start, h_start
template <class St>
struct FsaTypedBase : public FsaFeatureFunctionBase {
protected:
  typedef St State;
  static inline State & state(void *state) {
    return *(State*)state;
  }
  static inline State const& state(void const* state) {
    return *(State const*)state;
  }
  void set_starts(State const& s,State const& heuristic_s) {
    if (0) { // already in ctor
      start.resize(sizeof(State));
      h_start.resize(sizeof(State));
    }
    state(start.begin())=s;
    state(h_start.begin())=heuristic_s;
  }
  void set_h_start(State const& s) {
  }
public:
  int markov_order() const { return 1; }
  FsaTypedBase() : FsaFeatureFunctionBase(sizeof(State)) {
  }
};


// usage (if you're lazy):
// struct ShorterThanPrev : public FsaTypedBase<int>,FsaTypedScan<ShorterThanPrev>
template <class St,class Impl>
struct FsaTypedScan : public FsaTypedBase<St> {
  void Scan(SentenceMetadata const& smeta,WordID w,void const* st,void *next_state,FeatureVector *features) const {
    Impl const* impl=static_cast<Impl const*>(this);
    FSADBG("Scan "<<(*features)[impl->fid_]<<" = "<<impl->state(st)<<" ->"<<TD::Convert(w)<<" ");
    impl->ScanTyped(smeta,w,impl->state(st),impl->state(next_state),features);
    FSADBG(impl->state(next_state)<<" = "<<(*features)[impl->fid_]<<std::endl);
  }
};



// do not use if state size is 0, please.
const bool optimize_FsaScanner_zerostate=false;

template <class FF>
struct FsaScanner {
//  enum {ALIGN=8};
  static const int ALIGN=8;
  FF const& ff;
  SentenceMetadata const& smeta;
  int ssz;
  Bytes states; // first is at begin, second is at (char*)begin+stride
  void *st0; // states
  void *st1; // states+stride
  void *cs;
  inline void *nexts() const {
    return (cs==st0)?st1:st0;
  }
  FsaScanner(FF const& ff,SentenceMetadata const& smeta) : ff(ff),smeta(smeta)
  {
    ssz=ff.state_bytes();
    int stride=((ssz+ALIGN-1)/ALIGN)*ALIGN; // round up to multiple of ALIGN
    states.resize(stride+ssz);
    st0=states.begin();
    st1=(char*)st0+stride;
//    for (int i=0;i<2;++i) st[i]=cs+(i*stride);
  }
  void reset(void const* state) {
    cs=st0;
    std::memcpy(st0,state,ssz);
  }
  void scan(WordID w,FeatureVector *feat) {
    if (optimize_FsaScanner_zerostate && !ssz) {
      ff.Scan(smeta,w,0,0,feat);
      return;
    }
    void *ns=nexts();
    ff.Scan(smeta,w,cs,ns,feat);
    cs=ns;
  }

  void scan(WordID const* i,WordID const* end,FeatureVector *feat) {
#if 1
    // faster.
    if (optimize_FsaScanner_zerostate && !ssz)
      for (;i<end;++i)
        ff.Scan(smeta,*i,0,0,feat);
    else
      cs=FsaScan(ff,smeta,i,end,feat,cs,nexts());
#else
    for (;i<end;++i)
      scan(*i,feat);
#endif
  }
};


//TODO: combine 2 FsaFeatures typelist style (can recurse for more)




#endif
