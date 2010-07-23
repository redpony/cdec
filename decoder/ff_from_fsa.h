#ifndef FF_FROM_FSA_H
#define FF_FROM_FSA_H

#include "ff_fsa.h"

#define FSA_FF_DEBUG
#ifdef FSA_FF_DEBUG
# define FSAFFDBG(x) do { if (debug) { std::cerr << x; }  } while(0)
#else
# define FSAFFDBG(x)
#endif

/* regular bottom up scorer from Fsa feature
   uses guarantee about markov order=N to score ASAP
   encoding of state: if less than N-1 (ctxlen) words

   either:
   struct FF : public FsaImpl,FeatureFunctionFromFsa<FF> (more efficient)

   or:
   struct FF : public FsaFeatureFunctionDynamic,FeatureFunctionFromFsa<FF> (code sharing, but double dynamic dispatch)
*/

template <class Impl>
class FeatureFunctionFromFsa : public FeatureFunction {
  typedef void const* SP;
  typedef WordID *W;
  typedef WordID const* WP;
public:
  FeatureFunctionFromFsa(std::string const& param) : ff(param) {
    Init();
  }

  static std::string usage(bool args,bool verbose) {
    return Impl::usage(args,verbose);
  }

  Features features() const { return ff.features(); }

  //TODO: add source span to Fsa FF interface, pass along
  //TODO: read/debug VERY CAREFULLY
  void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                             const Hypergraph::Edge& edge,
                             const std::vector<const void*>& ant_contexts,
                             FeatureVector* features,
                             FeatureVector* estimated_features,
                             void* out_state) const
  {
    FSAFFDBG("(FromFsa) "<<name);
    ff.init_features(features); // estimated_features is fresh
    if (!ssz) {
      TRule const& rule=*edge.rule_;
      Sentence const& e = rule.e();
      for (int j = 0; j < e.size(); ++j) { // items in target side of rule
        if (e[j] < 1) { // variable
        } else {
          WordID ew=e[j];
          FSAFFDBG(' '<<TD::Convert(ew));
          ff.Scan(smeta,ew,0,0,features);
        }
      }
      FSAFFDBG('\n');
      return;
    }

    SP h_start=ff.heuristic_start_state();
    W left_begin=(W)out_state;
    W left_out=left_begin; // [left,fsa_state) = left ctx words.  if left words aren't full, then null wordid
    WP left_full=left_end_full(out_state);
    FsaScanner<Impl> fsa(ff,smeta);
    TRule const& rule=*edge.rule_;
    Sentence const& e = rule.e();
    for (int j = 0; j < e.size(); ++j) { // items in target side of rule
      if (e[j] < 1) { // variable
        SP a = ant_contexts[-e[j]];
        FSAFFDBG(' '<<describe_state(a));
        WP al=(WP)a;
        WP ale=left_end(a);
        // scan(al,le) these - the same as below else.  macro for now; pull into closure object later?
        int nw=ale-al;
        if (left_out+nw<left_full) { // nothing to score
          wordcpy(left_out,al,nw);
          left_out+=nw;
        } else if (left_out<left_full) { // something to score AND left context to fill
          int ntofill=left_full-left_out;
          wordcpy(left_out,al,ntofill);
          left_out=(W)left_full;
          // heuristic known now
          fsa.reset(h_start);
          fsa.scan(left_begin,left_full,estimated_features); // save heuristic (happens once only)
          al+=ntofill; // we used up the first ntofill words of al to end up in some known state via exactly M words total (M-ntofill were there beforehand).  now we can scan the remaining al words of this child
          goto scan;
        } else { // more to score / state to update
        scan:
          fsa.scan(al,ale,features);
        }
        if (nw>M) // child had full state already (had a "gap"); if nw==M then we already reached the same state via left word heuristic scan above
          fsa.reset(fsa_state(a));
      } else { // single word
        WordID ew=e[j];
        FSAFFDBG(' '<<TD::Convert(ew));
        // some redundancy: non-vectorized version of above handling of left words of child item
        if (left_out<left_full) {
          *left_out++=ew;
          if (left_out==left_full) { // handle heuristic, once only, establish state
            fsa.reset(h_start);
            fsa.scan(left_begin,left_full,estimated_features); // save heuristic (happens only once)
          }
        } else
          fsa.scan(ew,features);
      }
    }

    if (left_out<left_full) { // finally: partial heuristic fo runfilled items
      fsa.reset(h_start);
      fsa.scan(left_begin,left_out,estimated_features); // save heuristic (happens once)
      clear_fsa_state(out_state); // 0 bytes so we compare / hash correctly. don't know state yet
      while(left_out<left_full) *left_out++=TD::none; // mark as partial left word seq
    } else // or else store final right-state.  heuristic was already assigned
      fstatecpy(out_state,fsa.cs);
    FSAFFDBG(" = " << describe_state(out_state)<<" "<<(*features)[ff.fid()]<<" h="<<(*estimated_features)[ff.fid()]<<'\n');
  }

  void print_state(std::ostream &o,void const*ant) const {
    WP l=(WP)ant,le=left_end(ant),lf=left_end_full(ant);
    o<<'['<<Sentence(l,le);
    if (le==lf) {
      o<<" : ";
      ff.print_state(o,lf);
    }
    o << ']';
  }

  std::string describe_state(void const*ant) const {
    std::ostringstream o;
    print_state(o,ant);
    return o.str();
  }

  virtual void FinalTraversalFeatures(const SentenceMetadata& smeta,
                                      const void* residual_state,
                                      FeatureVector* final_features) const
  {
    ff.init_features(final_features); // estimated_features is fresh
    Sentence const& ends=ff.end_phrase();
    if (!ssz) {
      AccumFeatures(ff,smeta,begin(ends),end(ends),final_features,0);
      return;
    }
    SP ss=ff.start_state();
    WP l=(WP)residual_state,lend=left_end(residual_state);
    SP rst=fsa_state(residual_state);
    if (lend==rst) { // implying we have an fsa state
      AccumFeatures(ff,smeta,l,lend,final_features,ss); // e.g. <s> score(full left unscored phrase)
      AccumFeatures(ff,smeta,begin(ends),end(ends),final_features,rst); // e.g. [ctx for last M words] score("</s>")
    } else { // all we have is a single short phrase < M words before adding ends
      int nl=lend-l;
      Sentence whole(ends.size()+nl);
      WordID *w=begin(whole);
      wordcpy(w,l,nl);
      wordcpy(w+nl,begin(ends),ends.size());
      // whole = left-words + end-phrase
      AccumFeatures(ff,smeta,w,end(whole),final_features,ss);
    }
    FSAFFDBG("Final "<<name<<" = "<<*final_features<<'\n');
  }

  bool rule_feature() const {
    return StateSize()==0; // Fsa features don't get info about span
  }

  static void test() {
    WordID w1[1],w1b[1],w2[2];
    w1[0]=w2[0]=TD::Convert("hi");
    w2[1]=w1b[0]=TD::none;
    assert(left_end(w1,w1+1)==w1+1);
    assert(left_end(w1b,w1b+1)==w1b);
    assert(left_end(w2,w2+2)==w2+1);
  }

private:
  Impl ff;
  void Init() {
//    FeatureFunction::name=Impl::usage(false,false); // already achieved by ff_factory.cc
    M=ff.markov_order();
    ssz=ff.state_bytes();
    state_offset=sizeof(WordID)*M;
    SetStateSize(ff.state_bytes()+state_offset);
  }
  int M; // markov order (ctx len)
  FeatureFunctionFromFsa(); // not allowed.

  int state_offset; // store left-words first, then fsa state
  int ssz; // bytes in fsa state
  /*
    state layout: left WordIds, followed by fsa state
    left words have never been scored.  last ones remaining will be scored on FinalTraversalFeatures only.
    right state is unknown until we have all M left words (less than M means TD::none will pad out right end).  unk right state will be zeroed out for proper hash/equal recombination.
  */

  static inline WordID const* left_end(WordID const* left, WordID const* e) {
    for (;e>left;--e)
      if (e[-1]!=TD::none) break;
    //post: [left,e] are the seen left words
    return e;
  }
  inline WP left_end(SP ant) const {
    return left_end((WP)ant,(WP)fsa_state(ant));
  }
  inline WP left_end_full(SP ant) const {
    return (WP)fsa_state(ant);
  }
  inline SP fsa_state(SP ant) const {
    return ((char const*)ant+state_offset);
  }
  inline void *fsa_state(void * ant) const {
    return ((char *)ant+state_offset);
  }

  void clear_fsa_state(void *ant) const { // when state is unknown
    std::memset(fsa_state(ant),0,ssz);
  }

  inline void fstatecpy(void *ant,void const* src) const {
    std::memcpy(fsa_state(ant),src,ssz);
  }
};


#ifdef TEST_FSA
# include "tdict.cc"
# include "ff_sample_fsa.h"
int main() {
  std::cerr<<"Testing left_end...\n";
  std::cerr<<"sizeof(FeatureVector)="<<sizeof(FeatureVector)<<"\nsizeof(FeatureVectorList)="<<sizeof(FeatureVectorList)<<"\n";
  WordPenaltyFromFsa::test();
  return 0;
}
#endif

#endif
