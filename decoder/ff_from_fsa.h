#ifndef FF_FROM_FSA_H
#define FF_FROM_FSA_H

#include "ff_fsa.h"

#ifndef TD__none
// replacing dependency on SRILM
#define TD__none -1
#endif

#ifndef FSA_FF_DEBUG
# define FSA_FF_DEBUG 0
#endif
#if FSA_FF_DEBUG
# define FSAFFDBG(e,x) FSADBGif(debug(),e,x)
# define FSAFFDBGnl(e) FSADBGif_nl(debug(),e)
#else
# define FSAFFDBG(e,x)
# define FSAFFDBGnl(e)
#endif

/* regular bottom up scorer from Fsa feature
   uses guarantee about markov order=N to score ASAP
   encoding of state: if less than N-1 (ctxlen) words

   usage:
   typedef FeatureFunctionFromFsa<LanguageModelFsa> LanguageModelFromFsa;
*/

template <class Impl>
class FeatureFunctionFromFsa : public FeatureFunction {
  typedef void const* SP;
  typedef WordID *W;
  typedef WordID const* WP;
public:
  template <class I>
  FeatureFunctionFromFsa(I const& param) : ff(param) {
    debug_=true; // because factory won't set until after we construct.
  }
  template <class I>
  FeatureFunctionFromFsa(I & param) : ff(param) {
    debug_=true; // because factory won't set until after we construct.
  }

  static std::string usage(bool args,bool verbose) {
    return Impl::usage(args,verbose);
  }
  void init_name_debug(std::string const& n,bool debug) {
    FeatureFunction::init_name_debug(n,debug);
    ff.init_name_debug(n,debug);
  }

  // this should override
  Features features() const {
    DBGINIT("FeatureFunctionFromFsa features() name="<<ff.name()<<" features="<<FD::Convert(ff.features()));
    return ff.features();
  }

  // Log because it potentially stores info in edge.  otherwise the same as regular TraversalFeatures.
  void TraversalFeaturesLog(const SentenceMetadata& smeta,
                             Hypergraph::Edge& edge,
                             const std::vector<const void*>& ant_contexts,
                             FeatureVector* features,
                             FeatureVector* estimated_features,
                             void* out_state) const
  {
    TRule const& rule=*edge.rule_;
    Sentence const& e = rule.e();  // items in target side of rule
    typename Impl::Accum accum,h_accum;
    if (!ssz) { // special case for no state - but still build up longer phrases to score in case FSA overrides ScanPhraseAccum
      if (Impl::simple_phrase_score) {
        // save the effort of building up the contiguous rule phrases - probably can just use the else branch, now that phrases aren't copied but are scanned off e directly.
        for (int j=0,ee=e.size();j<ee;++j) {
          if (e[j]>=1) // token
            ff.ScanAccum(smeta,edge,(WordID)e[j],NULL,NULL,&accum);
          FSAFFDBG(edge," "<<TD::Convert(e[j]));
        }
      } else {
#undef RHS_WORD
#define RHS_WORD(j) (e[j]>=1)
        for (int j=0,ee=e.size();;++j) { // items in target side of rule
          for(;;++j) {
            if (j>=ee) goto rhs_done; // j may go 1 past ee due to k possibly getting to end
            if (RHS_WORD(j)) break;
          }
          // word @j
          int k=j;
          while(k<ee) if (!RHS_WORD(++k)) break;
          //end or nonword @k - [j,k) is phrase
          FSAFFDBG(edge," ["<<TD::GetString(&e[j],&e[k])<<']');
          ff.ScanPhraseAccum(smeta,edge,&e[j],&e[k],0,0,&accum);
          j=k;
        }
      }
    rhs_done:
      accum.Store(ff,features);
      FSAFFDBG(edge,"="<<accum.describe(ff));
      FSAFFDBGnl(edge);
      return;
    }

    // bear with me, because this is hard to understand.  reminder: ant_contexts and out_state are left-words first (up to M, TD__none padded).  if all M words are present, then FSA state follows.  otherwise 0 bytes to keep memcmp/hash happy.

//why do we compute heuristic in so many places?  well, because that's how we know what state we should score words in once we're full on our left context (because of markov order bound, we know the score will be the same no matter what came before that left context)
    // these left_* refer to our output (out_state):
    W left_begin=(W)out_state;
    W left_out=left_begin; // [left,fsa_state) = left ctx words.  if left words aren't full, then null wordid
    WP left_full=left_end_full(out_state);
    FsaScanner<Impl> fsa(ff,smeta,edge);
    /* fsa holds our current state once we've seen our first M rule or child left-context words.  that state scores up the rest of the words at the time, and is replaced by the right state of any full child.  at the end, if we've got at least M left words in all, it becomes our right state (otherwise, we don't bother storing the partial state, which might seem useful any time we're built on by a rule that has our variable in the initial position - but without also storing the heuristic for that case, we just end up rescanning from scratch anyway to produce the heuristic.  so we just store all 0 bytes if we have less than M left words at the end. */
    for (int j = 0,ee=e.size(); j < ee; ++j) { // items in target side of rule
    s_rhs_next:
      if (!RHS_WORD(j)) { // variable
        // variables a* are referring to this child derivation state.
        SP a = ant_contexts[-e[j]];
        WP al=(WP)a,ale=left_end(a); // the child left words
        int anw=ale-al;
        FSAFFDBG(edge,' '<<describe_state(a));
// anw left words in child.  full if == M.  we will use them to fill our left words, and then score the rest fully, knowing what state we're in based on h_state -> our left words -> any number of interior words which are scored then hidden
        if (left_out+anw<left_full) { // still short of M after adding - nothing to score (not even our heuristic)
          wordcpy(left_out,al,anw);
          left_out+=anw;
        } else if (left_out<left_full) { // we had less than M before, and will have a tleast M after adding.  so score heuristic and the rest M+1,... score inside.
          int ntofill=left_full-left_out;
          assert(ntofill==M-(left_out-left_begin));
          wordcpy(left_out,al,ntofill);
          left_out=(W)left_full;
          // heuristic known now
          fsa.reset(ff.heuristic_start_state());
          fsa.scan(left_begin,left_full,&h_accum); // save heuristic (happens once only)
          fsa.scan(al+ntofill,ale,&accum); // because of markov order, fully filled left words scored starting at h_start put us in the right state to score the extra words (which are forgotten)
          al+=ntofill; // we used up the first ntofill words of al to end up in some known state via exactly M words total (M-ntofill were there beforehand).  now we can scan the remaining al words of this child
        } else { // more to score / state to update (left already full)
          fsa.scan(al,ale,&accum);
        }
        if (anw==M)
          fsa.reset(fsa_state(a));
        // if child had full state already, we must assume there was a gap and use its right state (note: if the child derivation was exactly M words, then we still use its state even though it will be equal to our current; there's no way to distinguish between such an M word item and an e.g. 2*M+k word item, although it's extremely unlikely that you'd have a >M word item that happens to have the same left and right boundary words).
        assert(anw<=M); // of course, we never store more than M left words in an item.
      } else { // single word
        WordID ew=e[j];
        // some redundancy: non-vectorized version of above handling of left words of child item
        if (left_out<left_full) {
          *left_out++=ew;
          if (left_out==left_full) { // handle heuristic, once only, establish state
            fsa.reset(ff.heuristic_start_state());
            fsa.scan(left_begin,left_full,&h_accum); // save heuristic (happens only once)
          }
        } else {
          if (Impl::simple_phrase_score) {
            fsa.scan(ew,&accum); // single word scan isn't optimal if phrase is different
            FSAFFDBG(edge,' '<<TD::Convert(ew));
          } else {
            int k=j;
            while(k<ee) if (!RHS_WORD(++k)) break;
            FSAFFDBG(edge," rule-phrase["<<TD::GetString(&e[j],&e[k])<<']');
            fsa.scan(&e[j],&e[k],&accum);
            if (k==ee) goto s_rhs_done;
            j=k;
            goto s_rhs_next;
          }
        }
      }
    }
#undef RHS_WORD
  s_rhs_done:
    void *out_fsa_state=fsa_state(out_state);
    if (left_out<left_full) { // finally: partial heuristic for unfilled items
//      fsa.reset(ff.heuristic_start_state());      fsa.scan(left_begin,left_out,&h_accum);
      ff.ScanPhraseAccumOnly(smeta,edge,left_begin,left_out,ff.heuristic_start_state(),&h_accum);
      do { *left_out++=TD__none; } while(left_out<left_full); // none-terminate so left_end(out_state) will know how many words
      ff.state_zero(out_fsa_state); // so we compare / hash correctly. don't know state yet because left context isn't full
    } else // or else store final right-state.  heuristic was already assigned
      ff.state_copy(out_fsa_state,fsa.cs);
    accum.Store(ff,features);
    h_accum.Store(ff,estimated_features);
    FSAFFDBG(edge," = " << describe_state(out_state)<<" "<<name<<"="<<accum.describe(ff)<<" h="<<h_accum.describe(ff)<<")");
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
    Sentence const& ends=ff.end_phrase();
    typename Impl::Accum accum;
    if (!ssz) {
      FSAFFDBG(edge," (final,0state,end="<<ends<<")");
      ff.ScanPhraseAccumOnly(smeta,edge,begin(ends),end(ends),0,&accum);
    } else {
      SP ss=ff.start_state();
      WP l=(WP)residual_state,lend=left_end(residual_state);
      SP rst=fsa_state(residual_state);
      FSAFFDBG(edge," (final");// "<<name);//<< " before="<<*final_features);
      if (lend==rst) { // implying we have an fsa state
        ff.ScanPhraseAccumOnly(smeta,edge,l,lend,ss,&accum); // e.g. <s> score(full left unscored phrase)
        FSAFFDBG(edge," start="<<ff.describe_state(ss)<<"->{"<<Sentence(l,lend)<<"}");
        ff.ScanPhraseAccumOnly(smeta,edge,begin(ends),end(ends),rst,&accum); // e.g. [ctx for last M words] score("</s>")
        FSAFFDBG(edge," end="<<ff.describe_state(rst)<<"->{"<<ends<<"}");
      } else { // all we have is a single short phrase < M words before adding ends
        int nl=lend-l;
        Sentence whole(ends.size()+nl);
        WordID *wb=begin(whole);
        wordcpy(wb,l,nl);
        wordcpy(wb+nl,begin(ends),ends.size());
        FSAFFDBG(edge," whole={"<<whole<<"}");
        // whole = left-words + end-phrase
        ff.ScanPhraseAccumOnly(smeta,edge,wb,end(whole),ss,&accum);
      }
    }
    FSAFFDBG(edge,' '<<name<<"="<<accum.describe(ff));
    FSAFFDBGnl(edge);
    accum.Store(ff,final_features);
  }

  bool rule_feature() const {
    return StateSize()==0; // Fsa features don't get info about span
  }

  static void test() {
    WordID w1[1],w1b[1],w2[2];
    w1[0]=w2[0]=TD::Convert("hi");
    w2[1]=w1b[0]=TD__none;
    assert(left_end(w1,w1+1)==w1+1);
    assert(left_end(w1b,w1b+1)==w1b);
    assert(left_end(w2,w2+2)==w2+1);
  }

  // override from FeatureFunction; should be called by factory after constructor.  we'll also call in our own ctor
  void Init() {
    ff.Init();
    ff.sync();
    DBGINIT("base (single feature) FsaFeatureFunctionBase::Init name="<<name_<<" features="<<FD::Convert(features()));
//    FeatureFunction::name_=Impl::usage(false,false); // already achieved by ff_factory.cc
    M=ff.markov_order();
    ssz=ff.state_bytes();
    state_offset=sizeof(WordID)*M;
    SetStateSize(ssz+state_offset);
    assert(!ssz == !M); // no fsa state <=> markov order 0
  }

private:
  Impl ff;
  int M; // markov order (ctx len)
  FeatureFunctionFromFsa(); // not allowed.

  int state_offset; // NOTE: in bytes (add to char* only). store left-words first, then fsa state
  int ssz; // bytes in fsa state
  /*
    state layout: left WordIds, followed by fsa state
    left words have never been scored.  last ones remaining will be scored on FinalTraversalFeatures only.
    right state is unknown until we have all M left words (less than M means TD__none will pad out right end).  unk right state will be zeroed out for proper hash/equal recombination.
  */

  static inline WordID const* left_end(WordID const* left, WordID const* e) {
    for (;e>left;--e)
      if (e[-1]!=TD__none) break;
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
  std::cerr<<"sizeof(FeatureVector)="<<sizeof(FeatureVector)<<"\n";
  WordPenaltyFromFsa::test();
  return 0;
}
#endif

#endif
