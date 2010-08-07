#ifndef FF_FSA_DATA_H
#define FF_FSA_DATA_H

#include <stdint.h> //C99
#include <sstream>
#include "sentences.h"
#include "feature_accum.h"
#include "value_array.h"

typedef ValueArray<uint8_t> Bytes;

// stuff I see no reason to have virtual.  but there's a diamond inheritance problem to solve now when type erasing the CRTP impl wrapper.  virtual inheritance would slow things?
struct FsaFeatureFunctionData
{
  //HACK for diamond inheritance (w/o costing performance)
  FsaFeatureFunctionData *sync_to_;

  void sync() const { // call this if you modify any fields after your constructor is done
    if (sync_to_) *sync_to_=*this;
  }

  FsaFeatureFunctionData(int statesz=0,Sentence const& end_sentence_phrase=Sentence()) : ssz(statesz),start(statesz),h_start(statesz),end_phrase_(end_sentence_phrase) {
    debug_=true;
    sync_to_=0;
  }

  std::string name_;
  std::string name() const {
    return name_;
  }
  typedef SparseFeatureAccumulator Accum;
  bool debug_;
  bool debug() const { return debug_; }
  void state_copy(void *to,void const*from) const {
    if (ssz)
      std::memcpy(to,from,ssz);
  }
  void state_zero(void *st) const { // you should call this if you don't know the state yet and want it to be hashed/compared properly
    std::memset(st,0,ssz);
  }
  Features features() const {
    return features_;
  }
  int n_features() const {
    return features_.size();
  }
  int state_bytes() const { return ssz; } // or override this
  void const* start_state() const {
    return start.begin();
  }
  void const * heuristic_start_state() const {
    return h_start.begin();
  }
  Sentence const& end_phrase() const { return end_phrase_; }
  template <class T>
  static inline T* state_as(void *p) { return (T*)p; }
  template <class T>
  static inline T const* state_as(void const* p) { return (T*)p; }
  std::string describe_features(FeatureVector const& feats) {
    std::ostringstream o;
    o<<feats;
    return o.str();
  }
  void print_state(std::ostream &o,void const*state) const {
    char const* i=(char const*)state;
    char const* e=i+ssz;
    for (;i!=e;++i)
      print_hex_byte(o,*i);
  }

protected:
  Features features_;
  int ssz; // don't forget to set this. default 0 (it may depend on params of course)
  Bytes start,h_start; // start state and estimated-features (heuristic) start state.  set these.  default empty.
  Sentence end_phrase_; // words appended for final traversal (final state cost is assessed using Scan) e.g. "</s>" for lm.
  // this can be called instead or after constructor (also set bytes and end_phrase_)
  void set_state_bytes(int sb=0) {
    if (start.size()!=sb) start.resize(sb);
    if (h_start.size()!=sb) h_start.resize(sb);
    ssz=sb;
  }
  void set_end_phrase(WordID single) {
    end_phrase_=singleton_sentence(single);
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
  inline static void Add(Featval v,SingleFeatureAccumulator *a) {
    a->Add(v);
  }

};

#endif
