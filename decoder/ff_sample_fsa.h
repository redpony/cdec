#ifndef FF_SAMPLE_FSA_H
#define FF_SAMPLE_FSA_H

#include "ff_from_fsa.h"

// example: feature val = -1 * # of target words
struct WordPenaltyFsa : public FsaFeatureFunctionBase {
  static std::string usage(bool param,bool verbose) {
    return FeatureFunction::usage_helper(
      "WordPenaltyFsa","","-1 per target word"
      ,param,verbose);
  }

  WordPenaltyFsa(std::string const& param) {
    init_fid(usage(false,false));
    return;
    //below are all defaults:
    set_state_bytes(0);
    start.clear();
    h_start.clear();
  }
  static const float val_per_target_word=-1;
  // move from state to next_state after seeing word x, while emitting features->add_value(fid,val) possibly with duplicates.  state and next_state may be same memory.
  void Scan(SentenceMetadata const& smeta,WordID w,void const* state,void *next_state,FeatureVector *features) const {
    features->add_value(fid_,val_per_target_word);
  }
};

typedef FeatureFunctionFromFsa<WordPenaltyFsa> WordPenaltyFromFsa;


//
struct LongerThanPrev : public FsaFeatureFunctionBase {
  static std::string usage(bool param,bool verbose) {
    return FeatureFunction::usage_helper(
      "LongerThanPrev",
      "",
      "stupid example stateful (bigram) feature: -1 per target word that's longer than the previous word (always fires for first word of sentence)",
      param,verbose);
  }

  static inline int &wordlen(void *state) {
    return *(int*)state;
  }
  static inline int wordlen(void const* state) {
    return *(int const*)state;
  }
  static inline int wordlen(WordID w) {
    return std::strlen(TD::Convert(w));
  }
  int markov_order() const { return 1; }
  LongerThanPrev(std::string const& param) {
    init_fid(usage(false,false));
    set_state_bytes(sizeof(int));
//    start.resize(state_bytes()); // this is done by set_state_bytes already.
//    h_start.resize(state_bytes());
//    int ss=-1;
//    wordcpy((WordID*)start.begin(),&ss,&ss+1);
    //to_state(start.begin(),&ss,1);
    wordlen(start.begin())=-1;
    wordlen(h_start.begin())=4; // estimate: anything >4 chars is usually longer than previous
  }

  static const float val_per_target_word=-1;
  void Scan(SentenceMetadata const& smeta,WordID w,void const* state,void *next_state,FeatureVector *features) const {
    int prevlen=wordlen(state);
    int len=wordlen(w);
    wordlen(next_state)=len;
    if (len>prevlen)
      features->add_value(fid_,val_per_target_word);
  }

};

// similar example feature; base type exposes stateful type, defines markov_order 1, state size = sizeof(State)
struct ShorterThanPrev : FsaTypedScan<int,ShorterThanPrev> {
  static std::string usage(bool param,bool verbose) {
    return FeatureFunction::usage_helper(
      "ShorterThanPrev",
      "",
      "stupid example stateful (bigram) feature: -1 per target word that's shorter than the previous word (always fires for end of sentence)",
      param,verbose);
  }

  static inline int wordlen(WordID w) {
    return std::strlen(TD::Convert(w));
  }
  ShorterThanPrev(std::string const& param) {
    init_fid(usage(false,false));
//    end_phrase_.push_back(TD::Convert("")); // this triggers end of sentence firing
    set_starts(-1,4); // estimate: anything <4 chars is usually shorter than previous
  }

  static const float val_per_target_word=-1;
  // evil anti-google int & len out-param:
  void ScanTyped(SentenceMetadata const& smeta,WordID w,int prevlen,int &len,FeatureVector *features) const {
    len=wordlen(w);
    if (len<prevlen)
      features->add_value(fid_,val_per_target_word);
  }

  // already provided by FsaTypedScan<ShorterThanPrev>
/*  void Scan(SentenceMetadata const& smeta,WordID w,void const* st,void *next_state,FeatureVector *features) const {
    ScanTyped(smeta,w,state(st),state(next_state),features);
    } */

};


#endif
