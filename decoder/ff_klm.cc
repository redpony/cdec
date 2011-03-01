#include "ff_klm.h"

#include <cstring>

#include "hg.h"
#include "tdict.h"
#include "lm/enumerate_vocab.hh"

using namespace std;

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

  bool HasFullContext(const void *state) const {
    return *(static_cast<const char*>(state) + is_complete_offset_);
  }

  void SetHasFullContext(bool flag, void *state) const {
    *(static_cast<char*>(state) + is_complete_offset_) = flag;
  }

 public:
  double LookupWords(const TRule& rule, const vector<const void*>& ant_states, double* pest_sum, void* remnant) {
    double sum = 0.0;
    double est_sum = 0.0;
    int num_scored = 0;
    int num_estimated = 0;
    lm::ngram::State state = ngram_->NullContextState();
    const vector<WordID>& e = rule.e();
    bool context_complete = false;
    for (int j = 0; j < e.size(); ++j) {
      if (e[j] < 1) {   // handle non-terminal substitution
        const void* astate = (ant_states[-e[j]]);
        int unscored_ant_len = UnscoredSize(astate);
        for (int k = 0; k < unscored_ant_len; ++k) {
          const lm::WordIndex cur_word = IthUnscoredWord(k, astate);
          double p = 0;
          if (cur_word == kSOS_) {
            if (state.ValidLength() > 0) { p = -100; }
            state = ngram_->BeginSentenceState();
          } else {
            const lm::ngram::State scopy(state);
            p = ngram_->Score(scopy, cur_word, state);
          }
          ++num_scored;
          if (!context_complete) {
            if (num_scored >= order_) context_complete = true;
          }
          if (context_complete) {
            sum += p;
          } else {
            if (remnant)
              SetIthUnscoredWord(num_estimated, cur_word, remnant);
            ++num_estimated;
            est_sum += p;
          }
        }
        if (HasFullContext(astate)) { // this is equivalent to the "star" in Chiang 2007
          state = RemnantLMState(astate);
          context_complete = true;
        }
      } else {   // handle terminal
        const lm::WordIndex cur_word = MapWord(e[j]);
        double p = 0;
        if (cur_word == kSOS_) {
          if (state.ValidLength() > 0) p = -100;
          state = ngram_->BeginSentenceState();
        } else {
          const lm::ngram::State scopy(state);
          p = ngram_->Score(scopy, cur_word, state);
        }
        ++num_scored;
        if (!context_complete) {
          if (num_scored >= order_) context_complete = true;
        }
        if (context_complete) {
          sum += p;
        } else {
          if (remnant)
            SetIthUnscoredWord(num_estimated, cur_word, remnant);
          ++num_estimated;
          est_sum += p;
        }
      }
    }
    if (pest_sum) *pest_sum = est_sum;
    if (remnant) {
      state.ZeroRemaining();
      SetRemnantLMState(state, remnant);
      SetUnscoredSize(num_estimated, remnant);
      SetHasFullContext(context_complete || (num_scored >= order_), remnant);
    }
    return sum;
  }

  //FIXME: this assumes no target words on final unary -> goal rule.  is that ok?
  // for <s> (n-1 left words) and (n-1 right words) </s>
  double FinalTraversalCost(const void* state) {
    if (add_sos_eos_) {
      SetRemnantLMState(ngram_->BeginSentenceState(), dummy_state_);
      SetHasFullContext(1, dummy_state_);
      SetUnscoredSize(0, dummy_state_);
      dummy_ants_[1] = state;
      return LookupWords(*dummy_rule_, dummy_ants_, NULL, NULL);
    } else {
      // TODO, figure out whether spans are correct
      return 0;
    }
  }

  lm::WordIndex MapWord(WordID w) const {
    if (w >= map_.size())
      return 0;
    else
      return map_[w];
  }

 public:
  KLanguageModelImpl(const std::string& param) {
    add_sos_eos_ = true;
    string fname = param;
    if (param.find("-x ") == 0) {
       add_sos_eos_ = false;
       fname = param.substr(3);
    }
    lm::ngram::Config conf;
    VMapper vm(&map_);
    conf.enumerate_vocab = &vm; 
    ngram_ = new Model(fname.c_str(), conf);
    order_ = ngram_->Order();
    cerr << "Loaded " << order_ << "-gram KLM from " << fname << " (MapSize=" << map_.size() << ")\n";
    state_size_ = ngram_->StateSize() + 2 + (order_ - 1) * sizeof(lm::WordIndex);
    unscored_size_offset_ = ngram_->StateSize();
    is_complete_offset_ = unscored_size_offset_ + 1;
    unscored_words_offset_ = is_complete_offset_ + 1;

    // special handling of beginning / ending sentence markers
    dummy_state_ = new char[state_size_];
    dummy_ants_.push_back(dummy_state_);
    dummy_ants_.push_back(NULL);
    dummy_rule_.reset(new TRule("[DUMMY] ||| [BOS] [DUMMY] ||| [1] [2] </s> ||| X=0"));
    kSOS_ = MapWord(TD::Convert("<s>"));
    assert(kSOS_ > 0);
    kEOS_ = MapWord(TD::Convert("</s>"));
  }

  ~KLanguageModelImpl() {
    delete ngram_;
    delete[] dummy_state_;
  }

  int ReserveStateSize() const { return state_size_; }

 private:
  lm::WordIndex kSOS_;  // <s> - requires special handling.
  lm::WordIndex kEOS_;  // </s>
  Model* ngram_;
  bool add_sos_eos_; // flag indicating whether the hypergraph produces <s> and </s>
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
  vector<lm::WordIndex> map_;
  TRulePtr dummy_rule_;
};

template <class Model>
KLanguageModel<Model>::KLanguageModel(const string& param) {
  pimpl_ = new KLanguageModelImpl<Model>(param);
  fid_ = FD::Convert("LanguageModel");
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
  features->set_value(fid_, pimpl_->LookupWords(*edge.rule_, ant_states, &est, state));
  estimated_features->set_value(fid_, est);
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

