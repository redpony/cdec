#ifndef FF_FSA_H
#define FF_FSA_H

/*
  features whose score is just some PFSA over target string.  however, PFSA can use edge and smeta info (e.g. spans on edge) - not usually useful.

//SEE ALSO: ff_fsa_dynamic.h, ff_from_fsa.h

  state is some fixed width byte array.  could actually be a void *, WordID sequence, whatever.

  TODO: specify Scan return code or feature value = -inf for failure state (e.g. for hard intersection with desired target lattice?)

  TODO: maybe ff that wants to know about SentenceMetadata should store a ref to
  it permanently rather than get passed it for every operation.  we're never
  decoding more than 1 sentence at once and it's annoying to pass it.  same
  could apply for result edge as well since so far i only use it for logging
  when USE_INFO_EDGE 1 - would make the most sense if the same change happened
  to ff.h at the same time.

  TODO: there are a confusing array of default-implemented supposedly slightly more efficient overrides enabled; however, the two key differences are: do you score a phrase, or just word at a time (the latter constraining you to obey markov_order() everywhere.  you have to implement the word case no matter what.

  TODO: considerable simplification of implementation if Scan implementors are required to update state in place (using temporary copy if they need it), or e.g. using memmove (copy from end to beginning) to rotate state right.

  TODO: at what sizes is memcpy/memmove better than looping over 2-3 ints and assigning?

  TODO: fsa ff scores phrases not just words
  TODO: fsa feature aggregator that presents itself as a single fsa; benefit: when wrapped in ff_from_fsa, only one set of left words is stored.  downside: compared to separate ff, the inside portion of lower-order models is incorporated later.  however, the full heuristic is already available and exact for those words.  so don't sweat it.

  TODO: state (+ possibly span-specific) custom heuristic, e.g. in "longer than previous word" model, you can expect a higher outside if your state is a word of 2 letters.  this is on top of the nice heuristic for the unscored words, of course.  in ngrams, the avg prob will be about the same, but if the words possible for a source span are summarized, maybe it's possible to predict.  probably not worth the effort.
*/

#define FSA_DEBUG 0

#if USE_INFO_EDGE
#define FSA_DEBUG_CERR 0
#else
#define FSA_DEBUG_CERR 1
#endif

#define FSA_DEBUG_DEBUG 0
# define FSADBGif(i,e,x) do { if (i) { if (FSA_DEBUG_CERR){std::cerr<<x;}  INFO_EDGE(e,x); if (FSA_DEBUG_DEBUG){std::cerr<<"FSADBGif edge.info "<<&e<<" = "<<e.info()<<std::endl;}} } while(0)
# define FSADBGif_nl(i,e) do { if (i) { if (FSA_DEBUG_CERR) std::cerr<<std::endl; INFO_EDGE(e,"; "); } } while(0)
#if FSA_DEBUG
# include <iostream>
# define FSADBG(e,x) FSADBGif(d().debug(),e,x)
# define FSADBGnl(e) FSADBGif_nl(d().debug(),e,x)
#else
# define FSADBG(e,x)
# define FSADBGnl(e)
#endif

#include "fast_lexical_cast.hpp"
#include <sstream>
#include <string>
#include "ff.h"
#include "sparse_vector.h"
#include "tdict.h"
#include "hg.h"
#include "ff_fsa_data.h"

/*
usage: see ff_sample_fsa.h or ff_lm_fsa.h

 then, to decode, see ff_from_fsa.h (or TODO: left->right target-earley style rescoring)

 */


template <class Impl>
struct FsaFeatureFunctionBase : public FsaFeatureFunctionData {
  Impl const& d() const { return static_cast<Impl const&>(*this); }
  Impl & d()  { return static_cast<Impl &>(*this); }

  // this will get called by factory - override if you have multiple or dynamically named features.  note: may be called repeatedly
  void Init() {
    Init(name());
    DBGINIT("base (single feature) FsaFeatureFunctionBase::Init name="<<name()<<" features="<<FD::Convert(features_));
  }
  void Init(std::string const& fname) {
    fid_=FD::Convert(fname);
    InitHaveFid();
  }
  void InitHaveFid() {
    features_=FeatureFunction::single_feature(fid_);
  }
  Features features() const {
    DBGINIT("FeatureFunctionBase::features() name="<<name()<<" features="<<FD::Convert(features_));
    return features_;
  }

public:
  int fid_; // you can have more than 1 feature of course.

  std::string describe() const {
    std::ostringstream o;
    o<<*this;
    return o.str();
  }

  // can override to different return type, e.g. just return feats:
  Featval describe_features(FeatureVector const& feats) const {
    return feats.get(fid_);
  }

  bool debug() const { return true; }
  int fid() const { return fid_; } // return the one most important feature (for debugging)
  std::string name() const {
    return Impl::usage(false,false);
  }

  void print_state(std::ostream &o,void const*state) const {
    char const* i=(char const*)state;
    char const* e=i+ssz;
    for (;i!=e;++i)
      print_hex_byte(o,*i);
  }

  std::string describe_state(void const* state) const {
    std::ostringstream o;
    d().print_state(o,state);
    return o.str();
  }
  typedef SingleFeatureAccumulator Accum;

  // return m: all strings x with the same final m+1 letters must end in this state
  /* markov chain of order m: P(xn|xn-1...x1)=P(xn|xn-1...xn-m) */
  int markov_order() const { return 0; } // override if you use state.  order 0 implies state_bytes()==0 as well, as far as scoring/splitting is concerned (you can still track state, though)
  //TODO: if we wanted, we could mark certain states as maximal-context, but this would lose our fixed amount of left context in ff_from_fsa, and lose also our vector operations (have to scan left words 1 at a time, checking always to see where you change from h to inside - BUT, could detect equivalent LM states, which would be nice).



  // if [i,end) are unscored words of length <= markov_order, score some of them on the right, and return the number scored, i.e. [end-r,end) will have been scored for return r.  CAREFUL: for ngram you have to sometimes remember to pay all of the backoff once you see a few more words to the left.
  template <class Accum>
  int early_score_words(SentenceMetadata const& smeta,Hypergraph::Edge const& edge,WordID const* i, WordID const* end,Accum *accum) const {
    return 0;
  }

  // this isn't currently used at all.  this left-shortening is not recommended (wasn't worth the computation expense for ngram): specifically for bottom up scoring (ff_from_fsa), you can return a shorter left-words context - but this means e.g. for ngram tracking that a backoff occurred where the final BO cost isn't yet known.  you would also have to remember any necessary info in your own state - in the future, ff_from_fsa on a list of fsa features would only shorten it to the max


  // override this (static)
  static std::string usage(bool param,bool verbose) {
    return FeatureFunction::usage_helper("unnamed_fsa_feature","","",param,verbose);
  }

  // move from state to next_state after seeing word x, while emitting features->set_value(fid,val) possibly with duplicates.  state and next_state will never be the same memory.
  //TODO: decide if we want to require you to support dest same as src, since that's how we use it most often in ff_from_fsa bottom-up wrapper (in l->r scoring, however, distinct copies will be the rule), and it probably wouldn't be too hard for most people to support.  however, it's good to hide the complexity here, once (see overly clever FsaScan loop that swaps src/dest addresses repeatedly to scan a sequence by effectively swapping)

protected:
  // overrides have different name because of inheritance method hiding;

  // simple/common case; 1 fid.  these need not be overriden if you have multiple feature ids
  Featval Scan1(WordID w,void const* state,void *next_state) const {
    assert(0);
    return 0;
  }
  Featval Scan1Meta(SentenceMetadata const& /* smeta */,Hypergraph::Edge const& /* edge */,
                    WordID w,void const* state,void *next_state) const {
    return d().Scan1(w,state,next_state);
  }
public:

  // must override this or Scan1Meta or Scan1
  template <class Accum>
  inline void ScanAccum(SentenceMetadata const& smeta,Hypergraph::Edge const& edge,
                        WordID w,void const* state,void *next_state,Accum *a) const {
    Add(d().Scan1Meta(smeta,edge,w,state,next_state),a);
  }

  // bounce back and forth between two state vars starting at cs, returning end state location.  if we required src=dest addr safe state updating, this concept wouldn't need to exist.
  // required that you override this if you score phrases differently than word-by-word, however, you can just use the SCAN_PHRASE_ACCUM_OVERRIDE macro to do that in terms of ScanPhraseAccum
  template <class Accum>
  void *ScanPhraseAccumBounce(SentenceMetadata const& smeta,Hypergraph::Edge const& edge,WordID const* i, WordID const* end,void *cs,void *ns,Accum *accum) const {
    // extra code - IT'S FOR EFFICIENCY, MAN!  IT'S OK!  definitely no bugs here.
    if (!ssz) {
      for (;i<end;++i)
        d().ScanAccum(smeta,edge,*i,0,0,accum);
      return 0;
    }
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
      d().ScanAccum(smeta,edge,i[-1],es,os,accum); // e->o
    odd:
      d().ScanAccum(smeta,edge,i[0],os,es,accum); // o->e
    }
    return es;
  }


  static const bool simple_phrase_score=true; // if d().simple_phrase_score_, then you should expect different Phrase scores for phrase length > M.  so, set this false if you provide ScanPhraseAccum (SCAN_PHRASE_ACCUM_OVERRIDE macro does this)

  // override this (and use SCAN_PHRASE_ACCUM_OVERRIDE  ) if you want e.g. maximum possible order ngram scores with markov_order < n-1.  in the future SparseFeatureAccumulator will probably be the only option for type-erased FSA ffs.
  // note you'll still have to override ScanAccum
  template <class Accum>
  void ScanPhraseAccum(SentenceMetadata const& smeta,Hypergraph::Edge const & edge,
                              WordID const* i, WordID const* end,
                              void const* state,void *next_state,Accum *accum) const {
    if (!ssz) {
      for (;i<end;++i)
        d().ScanAccum(smeta,edge,*i,0,0,accum);
      return;
    }
    char tstate[ssz];
    void *tst=tstate;
    bool odd=(end-i)&1;
    void *cs,*ns;
    // we're going to use Bounce (word by word alternating of states) such that the final place is next_state
    if (odd) {
      cs=tst;
      ns=next_state;
    } else {
      cs=next_state;
      ns=tst;
    }
    state_copy(cs,state);
    void *est=d().ScanPhraseAccumBounce(smeta,edge,i,end,cs,ns,accum);
    assert(est==next_state);
  }



  // could replace this with a CRTP subclass providing these impls.
  // the d() subclass dispatch is not needed because these will be defined in the subclass
#define SCAN_PHRASE_ACCUM_OVERRIDE \
  static const bool simple_phrase_score=false; \
  template <class Accum> \
  void *ScanPhraseAccumBounce(SentenceMetadata const& smeta,Hypergraph::Edge const& edge,WordID const* i, WordID const* end,void *cs,void *ns,Accum *accum) const { \
    ScanPhraseAccum(smeta,edge,i,end,cs,ns,accum);  \
    return ns; \
  } \
  template <class Accum> \
  void ScanPhraseAccumOnly(SentenceMetadata const& smeta,Hypergraph::Edge const& edge, \
                              WordID const* i, WordID const* end, \
                              void const* state,Accum *accum) const { \
    char s2[ssz]; ScanPhraseAccum(smeta,edge,i,end,state,(void*)s2,accum); \
  }

  // override this or bounce along with above.  note: you can just call ScanPhraseAccum
  // doesn't set state (for heuristic in ff_from_fsa)
  template <class Accum>
  void ScanPhraseAccumOnly(SentenceMetadata const& smeta,Hypergraph::Edge const& edge,
                           WordID const* i, WordID const* end,
                           void const* state,Accum *accum) const {
    char s1[ssz];
    char s2[ssz];
    state_copy(s1,state);
    d().ScanPhraseAccumBounce(smeta,edge,i,end,(void*)s1,(void*)s2,accum);
  }

  // for single-feat only.  but will work for different accums
  template <class Accum>
  inline void Add(Featval v,Accum *a) const {
    a->Add(fid_,v);
  }
  inline void set_feat(FeatureVector *features,Featval v) const {
    features->set_value(fid_,v);
  }

  // don't set state-bytes etc. in ctor because it may depend on parsing param string
  FsaFeatureFunctionBase(int statesz=0,Sentence const& end_sentence_phrase=Sentence())
    : FsaFeatureFunctionData(statesz,end_sentence_phrase)
  {
    name_=name(); // should allow FsaDynamic wrapper to get name copied to it with sync
  }

};

template <class Impl>
struct MultipleFeatureFsa : public FsaFeatureFunctionBase<Impl> {
  typedef SparseFeatureAccumulator Accum;
};




// if State is pod.  sets state size and allocs start, h_start
// usage:
// struct ShorterThanPrev : public FsaTypedBase<int,ShorterThanPrev>
// i.e. Impl is a CRTP
template <class St,class Impl>
struct FsaTypedBase : public FsaFeatureFunctionBase<Impl> {
  Impl const& d() const { return static_cast<Impl const&>(*this); }
  Impl & d()  { return static_cast<Impl &>(*this); }
protected:
  typedef FsaFeatureFunctionBase<Impl> Base;
  typedef St State;
  static inline State & state(void *state) {
    return *(State*)state;
  }
  static inline State const& state(void const* state) {
    return *(State const*)state;
  }
  void set_starts(State const& s,State const& heuristic_s) {
    if (0) { // already in ctor
      Base::start.resize(sizeof(State));
      Base::h_start.resize(sizeof(State));
    }
    assert(Base::start.size()==sizeof(State));
    assert(Base::h_start.size()==sizeof(State));
    state(Base::start.begin())=s;
    state(Base::h_start.begin())=heuristic_s;
  }
  FsaTypedBase(St const& start_st=St()
               ,St const& h_start_st=St()
               ,Sentence const& end_sentence_phrase=Sentence())
    : Base(sizeof(State),end_sentence_phrase) {
    set_starts(start_st,h_start_st);
  }
public:
  void print_state(std::ostream &o,void const*st) const {
    o<<state(st);
  }
  int markov_order() const { return 1; }

  // override this
  Featval ScanT1S(WordID w,St const& /* from */ ,St & /* to */) const {
    return 0;
  }

  // or this
  Featval ScanT1(SentenceMetadata const& /* smeta */,Hypergraph::Edge const& /* edge */,WordID w,St const& from ,St & to) const {
    return d().ScanT1S(w,from,to);
  }

  // or this (most general)
  template <class Accum>
  inline void ScanT(SentenceMetadata const& smeta,Hypergraph::Edge const& edge,WordID w,St const& prev_st,St &new_st,Accum *a) const {
    Add(d().ScanT1(smeta,edge,w,prev_st,new_st),a);
  }

  // note: you're on your own when it comes to Phrase overrides.  see FsaFeatureFunctionBase.  sorry.

  template <class Accum>
  inline void ScanAccum(SentenceMetadata const& smeta,Hypergraph::Edge const& edge,WordID w,void const* st,void *next_state,Accum *a) const {
    Impl const& im=d();
    FSADBG(edge,"Scan "<<FD::Convert(im.fid_)<<" = "<<a->describe(im)<<" "<<im.state(st)<<"->"<<TD::Convert(w)<<" ");
    im.ScanT(smeta,edge,w,state(st),state(next_state),a);
    FSADBG(edge,state(next_state)<<" = "<<a->describe(im));
    FSADBGnl(edge);
  }
};


// keep a "current state" (bouncing back and forth)
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
  void *cs; // initially st0, alternates between st0 and st1
  inline void *nexts() const {
    return (cs==st0)?st1:st0;
  }
  Hypergraph::Edge const& edge;
  FsaScanner(FF const& ff,SentenceMetadata const& smeta,Hypergraph::Edge const& edge) : ff(ff),smeta(smeta),edge(edge)
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
  template <class Accum>
  void scan(WordID w,Accum *a) {
    void *ns=nexts();
    ff.ScanAccum(smeta,edge,w,cs,ns,a);
    cs=ns;
  }
  template <class Accum>
  void scan(WordID const* i,WordID const* end,Accum *a) {
    // faster. and allows greater-order excursions
    cs=ff.ScanPhraseAccumBounce(smeta,edge,i,end,cs,nexts(),a);
  }
};


//TODO: combine 2 FsaFeatures typelist style (can recurse for more)




#endif
