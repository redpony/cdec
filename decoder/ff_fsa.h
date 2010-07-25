#ifndef FF_FSA_H
#define FF_FSA_H

/*
  features whose score is just some PFSA over target string.  however, PFSA can use edge and smeta info (e.g. spans on edge) - not usually useful.

  state is some fixed width byte array.  could actually be a void *, WordID sequence, whatever.

*/

//SEE ALSO: ff_fsa_dynamic.h, ff_from_fsa.h

//TODO: decide whether to use init_features / add_value vs. summing elsewhere + set_value once (or inefficient for from_fsa: sum distinct feature_vectors.  but L->R if we only scan 1 word at a time, that's fine

//#define FSA_DEBUG

# define FSADBGae(e,x) std::cerr << x; INFO_EDGE(e,x);
#ifdef FSA_DEBUG
# include <iostream>
# define FSADBG(e,x) do { if (d().debug()) { FSADBGae(e,x) } } while(0)
# define FSADBGnl(e) do { if (d().debug) { std::cerr<<std::endl; INFO_EDGE(e,"; "); } } while(0)
#else
# define FSADBG(e,x)
# define FSADBGnl(e)
#endif

#include <boost/lexical_cast.hpp>
#include <sstream>
#include <stdint.h> //C99
#include <string>
#include "ff.h"
#include "sparse_vector.h"
#include "value_array.h" // used to hold state
#include "tdict.h"
#include "hg.h"
#include "sentences.h"

typedef ValueArray<uint8_t> Bytes;

// it's not necessary to inherit from this, but you probably should to save yourself some boilerplate.  defaults to no-state

// usage:
// struct FsaFeat : public FsaTypedBase<int,FsaFeat>
// i.e. Impl is a CRTP
template <class Impl>
struct FsaFeatureFunctionBase {
  Impl const& d() const { return static_cast<Impl const&>(*this); }
  Impl & d()  { return static_cast<Impl &>(*this); }
protected:
  int state_bytes_; // don't forget to set this. default 0 (it may depend on params of course)
  Bytes start,h_start; // start state and estimated-features (heuristic) start state.  set these.  default empty.
  Sentence end_phrase_; // words appended for final traversal (final state cost is assessed using Scan) e.g. "</s>" for lm.
  void set_state_bytes(int sb=0) {
    if (start.size()!=sb) start.resize(sb);
    if (h_start.size()!=sb) h_start.resize(sb);
    state_bytes_=sb;
  }
  int fid_; // you can have more than 1 feature of course.
  void Init() { // CALL THIS MANUALLY (because feature name(s) may depend on param
    fid_=FD::Convert(d().name());
  }

  inline void static to_state(void *state,char const* begin,char const* end) {
    std::memcpy(state,begin,end-begin);
  }
  inline void static to_state(void *state,char const* begin,int n) {
    std::memcpy(state,begin,n);
  }
  template <class T>
  inline void static to_state(void *state,T const* begin,int n=1) {
    to_state(state,(char const*)begin,n*sizeof(T));
  }
  template <class T>
  inline void static to_state(void *state,T const* begin,T const* end) {
    to_state(state,(char const*)begin,(char const*)end);
  }
  inline static char hexdigit(int i) {
    int j=i-10;
    return j>=0?'a'+j:'0'+i;
  }
  inline static void print_hex_byte(std::ostream &o,unsigned c) {
      o<<hexdigit(c>>4);
      o<<hexdigit(c&0x0f);
  }

public:
  bool debug() const { return true; }
  int fid() const { return fid_; } // return the one most important feature (for debugging)
  std::string name() const {
    return Impl::usage(false,false);
  }

  void print_state(std::ostream &o,void const*state) const {
    char const* i=(char const*)state;
    char const* e=i+state_bytes_;
    for (;i!=e;++i)
      print_hex_byte(o,*i);
  }

  std::string describe_state(void const* state) const {
    std::ostringstream o;
    d().print_state(o,state);
    return o.str();
  }

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

  // different name because of inheritance method hiding; simple/common case; 1 fid
  Featval Scan1(WordID w,void const* state,void *next_state) const {
    return 0;
  }

  // NOTE: if you want to e.g. track statistics, cache, whatever, cast const away or use mutable members
  inline void Scan(SentenceMetadata const& smeta,const Hypergraph::Edge& edge,WordID w,void const* state,void *next_state,FeatureVector *features) const {
    features->maybe_add(fid_,d().Scan1(w,state,next_state));
  }

  // don't set state-bytes etc. in ctor because it may depend on parsing param string
  FsaFeatureFunctionBase(int statesz=0,Sentence const& end_sentence_phrase=Sentence()) : state_bytes_(statesz),start(statesz),h_start(statesz),end_phrase_(end_sentence_phrase) {}

};

// init state is in cs; overwrite cs, ns repeatedly (alternatively).  return resulting state
template <class FsaFF>
void *FsaScan(FsaFF const& ff,SentenceMetadata const& smeta,const Hypergraph::Edge& edge,WordID const* i, WordID const* end,FeatureVector *features, void *cs,void *ns) {
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
    ff.Scan(smeta,edge,i[-1],es,os,features); // e->o
  odd:
    ff.Scan(smeta,edge,i[0],os,es,features); // o->e
  }
  return es;
}

// if you have a more efficient implementation for scanning a phrase than one word at a time (e.g. LM context using sliding window buffer rather than rotating through a fixed state size), you can override this
template <class FsaFF>
void Scan(FsaFF const& ff,SentenceMetadata const& smeta,const Hypergraph::Edge& edge,WordID const* i,WordID const* e,void const* state,void *next_state,FeatureVector *features) {
  int ssz=ff.state_bytes();
  if (!ssz) {
    for (;i<e;++i)
      ff.Scan(smeta,edge,*i,0,0,features);
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
  void *est=FsaScan(ff,smeta,edge,i,end,features,cs,ns);
  assert(est==next_state);
}


// like above Scan, but don't bother storing final state (for FinalTraversalFeatures)
template <class FF>
void AccumFeatures(FF const& ff,SentenceMetadata const& smeta,const Hypergraph::Edge& edge,WordID const* i, WordID const* end,FeatureVector *h_features,void const* start_state) {
  int ssz=ff.state_bytes();
  if (ssz) {
    Bytes state(ssz),state2(ssz);
    void *cs=state.begin(),*ns=state2.begin();
    memcpy(cs,start_state,ff.state_bytes());
    FsaScan(ff,smeta,edge,i,end,h_features,cs,ns);
  } else
    for (;i<end;++i)
      ff.Scan(smeta,edge,*i,0,0,h_features);
}

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
  Featval ScanT1(WordID w,int prevlen,int &len) const { return 0; }
  inline void ScanT(SentenceMetadata const& smeta,const Hypergraph::Edge& edge,WordID w,int prevlen,int &len,FeatureVector *features) const {
    features->maybe_add(d().fid_,d().ScanT1(w,prevlen,len));
  }

  inline void Scan(SentenceMetadata const& smeta,const Hypergraph::Edge& edge,WordID w,void const* st,void *next_state,FeatureVector *features) const {
    Impl const& im=d();
    FSADBG(edge,"Scan "<<FD::Convert(im.fid_)<<" = "<<(*features)[im.fid_]<<" "<<im.state(st)<<" ->"<<TD::Convert(w)<<" ");
    im.ScanT(smeta,edge,w,im.state(st),im.state(next_state),features);
    FSADBG(edge,im.state(next_state)<<" = "<<(*features)[im.fid_]);
    FSADBGnl(edge);
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
  const Hypergraph::Edge& edge;
  FsaScanner(FF const& ff,SentenceMetadata const& smeta,const Hypergraph::Edge& edge) : ff(ff),smeta(smeta),edge(edge)
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
      ff.Scan(smeta,edge,w,0,0,feat);
      return;
    }
    void *ns=nexts();
    ff.Scan(smeta,edge,w,cs,ns,feat);
    cs=ns;
  }

  void scan(WordID const* i,WordID const* end,FeatureVector *feat) {
#if 1
    // faster.
    if (optimize_FsaScanner_zerostate && !ssz)
      for (;i<end;++i)
        ff.Scan(smeta,edge,*i,0,0,feat);
    else
      cs=FsaScan(ff,smeta,edge,i,end,feat,cs,nexts());
#else
    for (;i<end;++i)
      scan(*i,feat);
#endif
  }
};


//TODO: combine 2 FsaFeatures typelist style (can recurse for more)




#endif
