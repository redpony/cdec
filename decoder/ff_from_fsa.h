#ifndef FF_FROM_FSA_H
#define FF_FROM_FSA_H

#include "ff_fsa.h"

#define FSA_FF_DEBUG 0
#if FSA_FF_DEBUG
# define FSAFFDBG(e,x) FSADBGif(debug,e,x)
# define FSAFFDBGnl(e) FSADBGif_nl(debug,e)
#else
# define FSAFFDBG(e,x)
# define FSAFFDBGnl(e)
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
    debug=true; // because factory won't set until after we construct.
    Init();
  }

  static std::string usage(bool args,bool verbose) {
    return Impl::usage(args,verbose);
  }

  Features features() const { return ff.features(); }

  // Log because it
  void TraversalFeaturesLog(const SentenceMetadata& smeta,
                             Hypergraph::Edge& edge,
                             const std::vector<const void*>& ant_contexts,
                             FeatureVector* features,
                             FeatureVector* estimated_features,
                             void* out_state) const
  {
    ff.init_features(features); // estimated_features is fresh
    if (!ssz) {
      TRule const& rule=*edge.rule_;
      Sentence const& e = rule.e();
      for (int j = 0; j < e.size(); ++j) { // items in target side of rule
        if (e[j] < 1) { // variable
        } else {
          WordID ew=e[j];
          FSAFFDBG(edge,' '<<TD::Convert(ew));
          ff.Scan(smeta,edge,ew,0,0,features);
        }
      }
      FSAFFDBGnl(edge);
      return;
    }
//why do we compute heuristic in so many places?  well, because that's how we know what state we should score words in once we're full on our left context (because of markov order bound, we know the score will be the same no matter what came before that left context)
    SP h_start=ff.heuristic_start_state();
    // these left_* refer to our output (out_state):
    W left_begin=(W)out_state;
    W left_out=left_begin; // [left,fsa_state) = left ctx words.  if left words aren't full, then null wordid
    WP left_full=left_end_full(out_state);
    FsaScanner<Impl> fsa(ff,smeta,edge); // this holds our current state and eventuallybecomes our right state if we saw enough words
    TRule const& rule=*edge.rule_;
    Sentence const& e = rule.e();
    for (int j = 0; j < e.size(); ++j) { // items in target side of rule
      if (e[j] < 1) { // variable
        SP a = ant_contexts[-e[j]]; // variables a* are referring to this child derivation state.
        FSAFFDBG(edge,' '<<describe_state(a));
        WP al=(WP)a;
        WP ale=left_end(a);
        // scan(al,le) these - the same as below else.  macro for now; pull into closure object later?
        int anw=ale-al;
// anw left words in child.  full if == M.  we will use them to fill our left words, and then score the rest fully, knowing what state we're in based on h_state -> our left words -> any number of interior words which are scored then hidden
        if (left_out+anw<left_full) { // nothing to score after adding
          wordcpy(left_out,al,anw);
          left_out+=anw;
        } else if (left_out<left_full) { // something to score AND newly full left context to fill
          int ntofill=left_full-left_out;
          assert(ntofill==M-(left_out-left_begin));
          wordcpy(left_out,al,ntofill);
          left_out=(W)left_full;
          // heuristic known now
          fsa.reset(h_start);
          fsa.scan(left_begin,left_full,estimated_features); // save heuristic (happens once only)
          fsa.scan(al+ntofill,ale,features); // because of markov order, fully filled left words scored starting at h_start put us in the right state to score the extra words (which are forgotten)
          al+=ntofill; // we used up the first ntofill words of al to end up in some known state via exactly M words total (M-ntofill were there beforehand).  now we can scan the remaining al words of this child
        } else { // more to score / state to update (left already full)
          fsa.scan(al,ale,features);
        }
        if (anw==M) // child had full state already
          fsa.reset(fsa_state(a));
        assert(anw<=M);
      } else { // single word
        WordID ew=e[j];
        FSAFFDBG(edge,' '<<TD::Convert(ew));
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

    void *out_fsa_state=fsa_state(out_state);
    if (left_out<left_full) { // finally: partial heuristic for unfilled items
      fsa.reset(h_start);
      fsa.scan(left_begin,left_out,estimated_features);
      do { *left_out++=TD::none; } while(left_out<left_full); // none-terminate so left_end(out_state) will know how many words
      ff.state_zero(out_fsa_state); // so we compare / hash correctly. don't know state yet because left context isn't full
    } else // or else store final right-state.  heuristic was already assigned
      ff.state_copy(out_fsa_state,fsa.cs);
    FSAFFDBG(edge," = " << describe_state(out_state)<<" "<<name<<"="<<ff.describe_features(*features)<<" h="<<ff.describe_features(*estimated_features)<<")");
    FSAFFDBGnl(edge);
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

  //FIXME: it's assumed that the final rule is just a unary no-target-terminal rewrite (same as ff_lm)
  virtual void FinalTraversalFeatures(const SentenceMetadata& smeta,
                                      Hypergraph::Edge& edge,
                                      const void* residual_state,
                                      FeatureVector* final_features) const
  {
    ff.init_features(final_features);
    Sentence const& ends=ff.end_phrase();
    if (!ssz) {
      AccumFeatures(ff,smeta,edge,begin(ends),end(ends),final_features,0);
      return;
    }
    SP ss=ff.start_state();
    WP l=(WP)residual_state,lend=left_end(residual_state);
    SP rst=fsa_state(residual_state);
    FSAFFDBG(edge," (final");// "<<name);//<< " before="<<*final_features);

    if (lend==rst) { // implying we have an fsa state
      AccumFeatures(ff,smeta,edge,l,lend,final_features,ss); // e.g. <s> score(full left unscored phrase)
      FSAFFDBG(edge," start="<<ff.describe_state(ss)<<"->{"<<Sentence(l,lend)<<"}");
      AccumFeatures(ff,smeta,edge,begin(ends),end(ends),final_features,rst); // e.g. [ctx for last M words] score("</s>")
      FSAFFDBG(edge," end="<<ff.describe_state(rst)<<"->{"<<ends<<"}");
    } else { // all we have is a single short phrase < M words before adding ends
      int nl=lend-l;
      Sentence whole(ends.size()+nl);
      WordID *w=begin(whole);
      wordcpy(w,l,nl);
      wordcpy(w+nl,begin(ends),ends.size());
      FSAFFDBG(edge," whole={"<<whole<<"}");
      // whole = left-words + end-phrase
      AccumFeatures(ff,smeta,edge,w,end(whole),final_features,ss);
    }
    FSAFFDBG(edge,' '<<name<<"="<<ff.describe_features(*final_features));
    FSAFFDBGnl(edge);
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
    SetStateSize(ssz+state_offset);
    assert(!ssz == !M); // no fsa state <=> markov order 0
  }
  int M; // markov order (ctx len)
  FeatureFunctionFromFsa(); // not allowed.

  int state_offset; // NOTE: in bytes (add to char* only). store left-words first, then fsa state
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
