#include "ff_klm.h"

#include <cstring>
#include <iostream>

#include <boost/scoped_ptr.hpp>

#include "filelib.h"
#include "stringlib.h"
#include "hg.h"
#include "tdict.h"
#include "lm/enumerate_vocab.hh"

using namespace std;

static const unsigned char HAS_FULL_CONTEXT = 1;
static const unsigned char HAS_EOS_ON_RIGHT = 2;
static const unsigned char MASK             = 7;

// -x : rules include <s> and </s>
// -n NAME : feature id is NAME
bool ParseLMArgs(string const& in, string* filename, string* mapfile, bool* explicit_markers, string* featname) {
  vector<string> const& argv=SplitOnWhitespace(in);
  *explicit_markers = false;
  *featname="LanguageModel";
  *mapfile = "";
#define LMSPEC_NEXTARG if (i==argv.end()) {            \
    cerr << "Missing argument for "<<*last<<". "; goto usage; \
    } else { ++i; }

  for (vector<string>::const_iterator last,i=argv.begin(),e=argv.end();i!=e;++i) {
    string const& s=*i;
    if (s[0]=='-') {
      if (s.size()>2) goto fail;
      switch (s[1]) {
      case 'x':
        *explicit_markers = true;
        break;
      case 'm':
        LMSPEC_NEXTARG; *mapfile=*i;
        break;
      case 'n':
        LMSPEC_NEXTARG; *featname=*i;
        break;
#undef LMSPEC_NEXTARG
      default:
      fail:
        cerr<<"Unknown KLanguageModel option "<<s<<" ; ";
        goto usage;
      }
    } else {
      if (filename->empty())
        *filename=s;
      else {
        cerr<<"More than one filename provided. ";
        goto usage;
      }
    }
  }
  if (!filename->empty())
    return true;
usage:
  cerr << "KLanguageModel is incorrect!\n";
  return false;
}

template <class Model>
string KLanguageModel<Model>::usage(bool /*param*/,bool /*verbose*/) {
  return "KLanguageModel";
}

struct VMapper : public lm::ngram::EnumerateVocab {
  VMapper(vector<lm::WordIndex>* out) : out_(out), kLM_UNKNOWN_TOKEN(0) { out_->clear(); }
  void Add(lm::WordIndex index, const StringPiece &str) {
    const WordID cdec_id = TD::Convert(str.as_string());
    if (cdec_id >= out_->size())
      out_->resize(cdec_id + 1, kLM_UNKNOWN_TOKEN);
    (*out_)[cdec_id] = index;
  }
  vector<lm::WordIndex>* out_;
  const lm::WordIndex kLM_UNKNOWN_TOKEN;
};

template <class Model>
class KLanguageModelImpl {

  // returns the number of unscored words at the left edge of a span
  inline int UnscoredSize(const void* state) const {
    return *(static_cast<const char*>(state) + unscored_size_offset_);
  }

  inline void SetUnscoredSize(int size, void* state) const {
    *(static_cast<char*>(state) + unscored_size_offset_) = size;
  }

  static inline const lm::ngram::State& RemnantLMState(const void* state) {
    return *static_cast<const lm::ngram::State*>(state);
  }

  inline void SetRemnantLMState(const lm::ngram::State& lmstate, void* state) const {
    // if we were clever, we could use the memory pointed to by state to do all
    // the work, avoiding this copy
    memcpy(state, &lmstate, ngram_->StateSize());
  }

  lm::WordIndex IthUnscoredWord(int i, const void* state) const {
    const lm::WordIndex* const mem = reinterpret_cast<const lm::WordIndex*>(static_cast<const char*>(state) + unscored_words_offset_);
    return mem[i];
  }

  void SetIthUnscoredWord(int i, lm::WordIndex index, void *state) const {
    lm::WordIndex* mem = reinterpret_cast<lm::WordIndex*>(static_cast<char*>(state) + unscored_words_offset_);
    mem[i] = index;
  }

  inline bool GetFlag(const void *state, unsigned char flag) const {
    return (*(static_cast<const char*>(state) + is_complete_offset_) & flag);
  }

  inline void SetFlag(bool on, unsigned char flag, void *state) const {
    if (on) {
      *(static_cast<char*>(state) + is_complete_offset_) |= flag;
    } else {
      *(static_cast<char*>(state) + is_complete_offset_) &= (MASK ^ flag);
    }
  }

  inline bool HasFullContext(const void *state) const {
    return GetFlag(state, HAS_FULL_CONTEXT);
  }

  inline void SetHasFullContext(bool flag, void *state) const {
    SetFlag(flag, HAS_FULL_CONTEXT, state);
  }

 public:
  double LookupWords(const TRule& rule, const vector<const void*>& ant_states, double* pest_sum, double* oovs, double* est_oovs, void* remnant) {
    double sum = 0.0;
    double est_sum = 0.0;
    int num_scored = 0;
    int num_estimated = 0;
    if (oovs) *oovs = 0;
    if (est_oovs) *est_oovs = 0;
    bool saw_eos = false;
    bool has_some_history = false;
    lm::ngram::State state = ngram_->NullContextState();
    const vector<WordID>& e = rule.e();
    bool context_complete = false;
    for (int j = 0; j < e.size(); ++j) {
      if (e[j] < 1) {   // handle non-terminal substitution
        const void* astate = (ant_states[-e[j]]);
        int unscored_ant_len = UnscoredSize(astate);
        for (int k = 0; k < unscored_ant_len; ++k) {
          const lm::WordIndex cur_word = IthUnscoredWord(k, astate);
          const bool is_oov = (cur_word == 0);
          double p = 0;
          if (cur_word == kSOS_) {
            state = ngram_->BeginSentenceState();
            if (has_some_history) {  // this is immediately fully scored, and bad
              p = -100;
              context_complete = true;
            } else {  // this might be a real <s>
              num_scored = max(0, order_ - 2);
            }
          } else {
            const lm::ngram::State scopy(state);
            p = ngram_->Score(scopy, cur_word, state);
            if (saw_eos) { p = -100; }
            saw_eos = (cur_word == kEOS_);
          }
          has_some_history = true;
          ++num_scored;
          if (!context_complete) {
            if (num_scored >= order_) context_complete = true;
          }
          if (context_complete) {
            sum += p;
            if (oovs && is_oov) (*oovs)++;
          } else {
            if (remnant)
              SetIthUnscoredWord(num_estimated, cur_word, remnant);
            ++num_estimated;
            est_sum += p;
            if (est_oovs && is_oov) (*est_oovs)++;
          }
        }
        saw_eos = GetFlag(astate, HAS_EOS_ON_RIGHT);
        if (HasFullContext(astate)) { // this is equivalent to the "star" in Chiang 2007
          state = RemnantLMState(astate);
          context_complete = true;
        }
      } else {   // handle terminal
        const WordID cdec_word_or_class = ClassifyWordIfNecessary(e[j]);  // in future,
                                                                          // maybe handle emission
        const lm::WordIndex cur_word = MapWord(cdec_word_or_class); // map to LM's id
        double p = 0;
        const bool is_oov = (cur_word == 0);
        if (cur_word == kSOS_) {
          state = ngram_->BeginSentenceState();
          if (has_some_history) {  // this is immediately fully scored, and bad
            p = -100;
            context_complete = true;
          } else {  // this might be a real <s>
            num_scored = max(0, order_ - 2);
          }
        } else {
          const lm::ngram::State scopy(state);
          p = ngram_->Score(scopy, cur_word, state);
          if (saw_eos) { p = -100; }
          saw_eos = (cur_word == kEOS_);
        }
        has_some_history = true;
        ++num_scored;
        if (!context_complete) {
          if (num_scored >= order_) context_complete = true;
        }
        if (context_complete) {
          sum += p;
          if (oovs && is_oov) (*oovs)++;
        } else {
          if (remnant)
            SetIthUnscoredWord(num_estimated, cur_word, remnant);
          ++num_estimated;
          est_sum += p;
          if (est_oovs && is_oov) (*est_oovs)++;
        }
      }
    }
    if (pest_sum) *pest_sum = est_sum;
    if (remnant) {
      state.ZeroRemaining();
      SetFlag(saw_eos, HAS_EOS_ON_RIGHT, remnant);
      SetRemnantLMState(state, remnant);
      SetUnscoredSize(num_estimated, remnant);
      SetHasFullContext(context_complete || (num_scored >= order_), remnant);
    }
    return sum;
  }

  // this assumes no target words on final unary -> goal rule.  is that ok?
  // for <s> (n-1 left words) and (n-1 right words) </s>
  double FinalTraversalCost(const void* state) {
    if (add_sos_eos_) {  // rules do not produce <s> </s>, so do it here
      SetRemnantLMState(ngram_->BeginSentenceState(), dummy_state_);
      SetHasFullContext(1, dummy_state_);
      SetUnscoredSize(0, dummy_state_);
      dummy_ants_[1] = state;
      return LookupWords(*dummy_rule_, dummy_ants_, NULL, NULL, NULL, NULL);
    } else {  // rules DO produce <s> ... </s>
      double p = 0;
      if (!GetFlag(state, HAS_EOS_ON_RIGHT)) { p -= 100; }
      if (UnscoredSize(state) > 0) {  // are there unscored words
        if (kSOS_ != IthUnscoredWord(0, state)) {
          p -= 100 * UnscoredSize(state);
        }
      }
      return p;
    }
  }

  // if this is not a class-based LM, returns w untransformed,
  // otherwise returns a word class mapping of w,
  // returns TD::Convert("<unk>") if there is no mapping for w
  WordID ClassifyWordIfNecessary(WordID w) const {
    if (word2class_map_.empty()) return w;
    if (w >= word2class_map_.size())
      return kCDEC_UNK;
    else
      return word2class_map_[w];
  }

  // converts to cdec word id's to KenLM's id space, OOVs and <unk> end up at 0
  lm::WordIndex MapWord(WordID w) const {
    if (w >= cdec2klm_map_.size())
      return 0;
    else
      return cdec2klm_map_[w];
  }

 public:
  KLanguageModelImpl(const string& filename, const string& mapfile, bool explicit_markers) :
      kCDEC_UNK(TD::Convert("<unk>")) ,
      add_sos_eos_(!explicit_markers) {
    if (true) {
      boost::scoped_ptr<lm::ngram::EnumerateVocab> vm;
      vm.reset(new VMapper(&cdec2klm_map_));
      lm::ngram::Config conf;
      conf.enumerate_vocab = vm.get(); 
      ngram_ = new Model(filename.c_str(), conf);
    }
    order_ = ngram_->Order();
    cerr << "Loaded " << order_ << "-gram KLM from " << filename << " (MapSize=" << cdec2klm_map_.size() << ")\n";
    state_size_ = ngram_->StateSize() + 2 + (order_ - 1) * sizeof(lm::WordIndex);
    unscored_size_offset_ = ngram_->StateSize();
    is_complete_offset_ = unscored_size_offset_ + 1;
    unscored_words_offset_ = is_complete_offset_ + 1;

    // special handling of beginning / ending sentence markers
    dummy_state_ = new char[state_size_];
    memset(dummy_state_, 0, state_size_);
    dummy_ants_.push_back(dummy_state_);
    dummy_ants_.push_back(NULL);
    dummy_rule_.reset(new TRule("[DUMMY] ||| [BOS] [DUMMY] ||| [1] [2] </s> ||| X=0"));
    kSOS_ = MapWord(TD::Convert("<s>"));
    assert(kSOS_ > 0);
    kEOS_ = MapWord(TD::Convert("</s>"));
    assert(kEOS_ > 0);
    assert(MapWord(kCDEC_UNK) == 0); // KenLM invariant

    // handle class-based LMs (unambiguous word->class mapping reqd.)
    if (mapfile.size())
      LoadWordClasses(mapfile);
  }

  void LoadWordClasses(const string& file) {
    ReadFile rf(file);
    istream& in = *rf.stream();
    string line;
    vector<WordID> dummy;
    int lc = 0;
    cerr << "  Loading word classes from " << file << " ...\n";
    AddWordToClassMapping_(TD::Convert("<s>"), TD::Convert("<s>"));
    AddWordToClassMapping_(TD::Convert("</s>"), TD::Convert("</s>"));
    while(in) {
      getline(in, line);
      if (!in) continue;
      dummy.clear();
      TD::ConvertSentence(line, &dummy);
      ++lc;
      if (dummy.size() != 2) {
        cerr << "    Format error in " << file << ", line " << lc << ": " << line << endl;
        abort();
      }
      AddWordToClassMapping_(dummy[0], dummy[1]);
    }
  }

  void AddWordToClassMapping_(WordID word, WordID cls) {
    if (word2class_map_.size() <= word) {
      word2class_map_.resize((word + 10) * 1.1, kCDEC_UNK);
      assert(word2class_map_.size() > word);
    }
    if(word2class_map_[word] != kCDEC_UNK) {
      cerr << "Multiple classes for symbol " << TD::Convert(word) << endl;
      abort();
    }
    word2class_map_[word] = cls;
  }

  ~KLanguageModelImpl() {
    delete ngram_;
    delete[] dummy_state_;
  }

  int ReserveStateSize() const { return state_size_; }

 private:
  const WordID kCDEC_UNK;
  lm::WordIndex kSOS_;  // <s> - requires special handling.
  lm::WordIndex kEOS_;  // </s>
  Model* ngram_;
  const bool add_sos_eos_; // flag indicating whether the hypergraph produces <s> and </s>
                     // if this is true, FinalTransitionFeatures will "add" <s> and </s>
                     // if false, FinalTransitionFeatures will score anything with the
                     // markers in the right place (i.e., the beginning and end of
                     // the sentence) with 0, and anything else with -100

  int order_;
  int state_size_;
  int unscored_size_offset_;
  int is_complete_offset_;
  int unscored_words_offset_;
  char* dummy_state_;
  vector<const void*> dummy_ants_;
  vector<lm::WordIndex> cdec2klm_map_;
  vector<WordID> word2class_map_;        // if this is a class-based LM, this is the word->class mapping
  TRulePtr dummy_rule_;
};

template <class Model>
KLanguageModel<Model>::KLanguageModel(const string& param) {
  string filename, mapfile, featname;
  bool explicit_markers;
  if (!ParseLMArgs(param, &filename, &mapfile, &explicit_markers, &featname)) {
    abort();
  }
  pimpl_ = new KLanguageModelImpl<Model>(filename, mapfile, explicit_markers);
  fid_ = FD::Convert(featname);
  oov_fid_ = FD::Convert(featname+"_OOV");
  SetStateSize(pimpl_->ReserveStateSize());
}

template <class Model>
Features KLanguageModel<Model>::features() const {
  return single_feature(fid_);
}

template <class Model>
KLanguageModel<Model>::~KLanguageModel() {
  delete pimpl_;
}

template <class Model>
void KLanguageModel<Model>::TraversalFeaturesImpl(const SentenceMetadata& /* smeta */,
                                          const Hypergraph::Edge& edge,
                                          const vector<const void*>& ant_states,
                                          SparseVector<double>* features,
                                          SparseVector<double>* estimated_features,
                                          void* state) const {
  double est = 0;
  double oovs = 0;
  double est_oovs = 0;
  features->set_value(fid_, pimpl_->LookupWords(*edge.rule_, ant_states, &est, &oovs, &est_oovs, state));
  estimated_features->set_value(fid_, est);
  if (oov_fid_) {
    if (oovs) features->set_value(oov_fid_, oovs);
    if (est_oovs) estimated_features->set_value(oov_fid_, est_oovs);
  }
}

template <class Model>
void KLanguageModel<Model>::FinalTraversalFeatures(const void* ant_state,
                                           SparseVector<double>* features) const {
  features->set_value(fid_, pimpl_->FinalTraversalCost(ant_state));
}

// instantiate templates
template class KLanguageModel<lm::ngram::ProbingModel>;
template class KLanguageModel<lm::ngram::SortedModel>;
template class KLanguageModel<lm::ngram::TrieModel>;

