#ifndef FF_SAMPLE_FSA_H
#define FF_SAMPLE_FSA_H

#include "ff_from_fsa.h"

// example: feature val = 1 * # of target words
struct WordPenaltyFsa : public FsaFeatureFunctionBase<WordPenaltyFsa> {
  static std::string usage(bool param,bool verbose) {
    return FeatureFunction::usage_helper(
      "WordPenaltyFsa","","1 per target word"
      ,param,verbose);
  }

  WordPenaltyFsa(std::string const& param) {
    Init();
    return;
    //below are all defaults:
    set_state_bytes(0);
    start.clear();
    h_start.clear();
  }
  // move from state to next_state after seeing word x, while emitting features->add_value(fid,val) possibly with duplicates.  state and next_state may be same memory.
  Featval Scan1(WordID w,void const* state,void *next_state) const {
    return 1;
  }
};

typedef FeatureFunctionFromFsa<WordPenaltyFsa> WordPenaltyFromFsa;

struct SameFirstLetter : public FsaFeatureFunctionBase<SameFirstLetter> {
  SameFirstLetter(std::string const& param) : FsaFeatureFunctionBase<SameFirstLetter>(1,singleton_sentence("END")) {  start[0]='a';h_start[0]=0; } // 1 byte of state, scan final (single) symbol "END" to get final state cost
  int markov_order() const { return 1; }
  Featval Scan1(WordID w,void const* old_state,void *new_state) const {
    char cw=TD::Convert(w)[0];
    char co=*(char const*)old_state;
    *(char *)new_state = cw;
    return cw==co?1:0;
  }
  void print_state(std::ostream &o,void const* st) const {
    o<<*(char const*)st;
  }
  static std::string usage(bool param,bool verbose) {
    return FeatureFunction::usage_helper("SameFirstLetter","[no args]","1 each time 2 consecutive words start with the same letter",param,verbose);
  }
};


// appears to be buggy right now: give it a bonus weight (+)
struct LongerThanPrev : public FsaFeatureFunctionBase<LongerThanPrev> {
  typedef FsaFeatureFunctionBase<LongerThanPrev> Base;
  static std::string usage(bool param,bool verbose) {
    return FeatureFunction::usage_helper(
      "LongerThanPrev",
      "",
      "stupid example stateful (bigram) feature: 1 per target word that's longer than the previous word (<s> sentence begin considered 3 chars long, </s> is sentence end.)",
      param,verbose);
  }

  static inline int &state(void *st) {
    return *(int*)st;
  }
  static inline int state(void const* st) {
    return *(int const*)st;
  }
/*  int describe_state(void const* st) const {
    return state(st);
  }
*/
  // only need 1 of the 2
  void print_state(std::ostream &o,void const* st) const {
    o<<state(st);
  }

  static inline int wordlen(WordID w) {
    return std::strlen(TD::Convert(w));
  }
  int markov_order() const { return 1; }
  LongerThanPrev(std::string const& param) : Base(sizeof(int)/* ,singleton_sentence(TD::se) */) {
    Init();
    if (0) { // all this is done in constructor already
      set_state_bytes(sizeof(int));
      //start.resize(state_bytes());h_start.resize(state_bytes()); // this is done by set_state_bytes already.
      int ss=3;
      to_state(start.begin(),&ss,1);
      ss=4;
      to_state(h_start.begin(),&ss,1);
    }
    assert(state_bytes()==sizeof(int));
    assert(start.size()==sizeof(int));
    assert(h_start.size()==sizeof(int));
    state(start.begin())=999999;
    state(h_start.begin())=4; // estimate: anything >4 chars is usually longer than previous

  }

  Featval Scan1(WordID w,void const* from,void *next_state) const {
    int prevlen=state(from);
    int len=wordlen(w);
    state(next_state)=len;
    return len>prevlen ? 1 : 0;
  }
};

// similar example feature; base type exposes stateful type, defines markov_order 1, state size = sizeof(State)
// also buggy right now: give it a bonus weight
struct ShorterThanPrev : FsaTypedBase<int,ShorterThanPrev> {
  typedef FsaTypedBase<int,ShorterThanPrev> Base;
  static std::string usage(bool param,bool verbose) {
    return FeatureFunction::usage_helper(
      "ShorterThanPrev",
      "",
      "stupid example stateful (bigram) feature: 1 per target word that's shorter than the previous word (end of sentence considered '</s>')",
      param,verbose);
  }

  static inline int wordlen(WordID w) {
    return std::strlen(TD::Convert(w));
  }
  ShorterThanPrev(std::string const& param)
  : Base(-1,4/* ,singleton_sentence(TD::se) */)
    // start, h_start, end_phrase
    // estimate: anything <4 chars is usually shorter than previous
  {
    Init();
  }


/*  Featval ScanT1(WordID w,int prevlen,int &len) const;
    // alternative to below:
    */

  // evil anti-google int & len out-param:
  void ScanT(SentenceMetadata const& /* smeta */,const Hypergraph::Edge& /* edge */,WordID w,int prevlen,int &len,FeatureVector *features) const {
    len=wordlen(w);
    if (len<prevlen)
      features->add_value(fid_,1);
  }

};


#endif
