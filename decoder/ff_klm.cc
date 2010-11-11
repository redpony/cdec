#include "ff_klm.h"

#include "hg.h"
#include "tdict.h"
#include "lm/model.hh"
#include "lm/enumerate_vocab.hh"

using namespace std;

string KLanguageModel::usage(bool param,bool verbose) {
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

class KLanguageModelImpl {
  inline int StateSize(const void* state) const {
    return *(static_cast<const char*>(state) + state_size_);
  }

  inline void SetStateSize(int size, void* state) const {
    *(static_cast<char*>(state) + state_size_) = size;
  }

#if 0
  virtual double WordProb(WordID word, WordID const* context) {
    return ngram_.wordProb(word, (VocabIndex*)context);
  }

  // may be shorter than actual null-terminated length.  context must be null terminated.  len is just to save effort for subclasses that don't support contextID
  virtual int ContextSize(WordID const* context,int len) {
    unsigned ret;
    ngram_.contextID((VocabIndex*)context,ret);
    return ret;
  }
  virtual double ContextBOW(WordID const* context,int shortened_len) {
    return ngram_.contextBOW((VocabIndex*)context,shortened_len);
  }

  inline double LookupProbForBufferContents(int i) {
//    int k = i; cerr << "P("; while(buffer_[k] > 0) { std::cerr << TD::Convert(buffer_[k++]) << " "; }
    double p = WordProb(buffer_[i], &buffer_[i+1]);
    if (p < floor_) p = floor_;
//    cerr << ")=" << p << endl;
    return p;
  }

  string DebugStateToString(const void* state) const {
    int len = StateSize(state);
    const int* astate = reinterpret_cast<const int*>(state);
    string res = "[";
    for (int i = 0; i < len; ++i) {
      res += " ";
      res += TD::Convert(astate[i]);
    }
    res += " ]";
    return res;
  }

  inline double ProbNoRemnant(int i, int len) {
    int edge = len;
    bool flag = true;
    double sum = 0.0;
    while (i >= 0) {
      if (buffer_[i] == kSTAR) {
        edge = i;
        flag = false;
      } else if (buffer_[i] <= 0) {
        edge = i;
        flag = true;
      } else {
        if ((edge-i >= order_) || (flag && !(i == (len-1) && buffer_[i] == kSTART)))
          sum += LookupProbForBufferContents(i);
      }
      --i;
    }
    return sum;
  }

  double EstimateProb(const vector<WordID>& phrase) {
    int len = phrase.size();
    buffer_.resize(len + 1);
    buffer_[len] = kNONE;
    int i = len - 1;
    for (int j = 0; j < len; ++j,--i)
      buffer_[i] = phrase[j];
    return ProbNoRemnant(len - 1, len);
  }

  //TODO: make sure this doesn't get used in FinalTraversal, or if it does, that it causes no harm.

  //TODO: use stateless_cost instead of ProbNoRemnant, check left words only.  for items w/ fewer words than ctx len, how are they represented?  kNONE padded?

  //Vocab_None is (unsigned)-1 in srilm, same as kNONE. in srilm (-1), or that SRILM otherwise interprets -1 as a terminator and not a word
  double EstimateProb(const void* state) {
    if (unigram) return 0.;
    int len = StateSize(state);
    //  << "residual len: " << len << endl;
    buffer_.resize(len + 1);
    buffer_[len] = kNONE;
    const int* astate = reinterpret_cast<const WordID*>(state);
    int i = len - 1;
    for (int j = 0; j < len; ++j,--i)
      buffer_[i] = astate[j];
    return ProbNoRemnant(len - 1, len);
  }

  //FIXME: this assumes no target words on final unary -> goal rule.  is that ok?
  // for <s> (n-1 left words) and (n-1 right words) </s>
  double FinalTraversalCost(const void* state) {
    if (unigram) return 0.;
    int slen = StateSize(state);
    int len = slen + 2;
    // cerr << "residual len: " << len << endl;
    buffer_.resize(len + 1);
    buffer_[len] = kNONE;
    buffer_[len-1] = kSTART;
    const int* astate = reinterpret_cast<const WordID*>(state);
    int i = len - 2;
    for (int j = 0; j < slen; ++j,--i)
      buffer_[i] = astate[j];
    buffer_[i] = kSTOP;
    assert(i == 0);
    return ProbNoRemnant(len - 1, len);
  }

  /// just how SRILM likes it: [rbegin,rend) is a phrase in reverse word order and null terminated so *rend=kNONE.  return unigram score for rend[-1] plus
  /// cost returned is some kind of log prob (who cares, we're just adding)
  double stateless_cost(WordID *rbegin,WordID *rend) {
    UNIDBG("p(");
    double sum=0;
    for (;rend>rbegin;--rend) {
      sum+=clamp(WordProb(rend[-1],rend));
      UNIDBG(" "<<TD::Convert(rend[-1]));
    }
    UNIDBG(")="<<sum<<endl);
    return sum;
  }

  //TODO: this would be a fine rule heuristic (for reordering hyperedges prior to rescoring.  for now you can just use a same-lm-file -o 1 prelm-rescore :(
  double stateless_cost(TRule const& rule) {
    //TODO: make sure this is correct.
    int len = rule.ELength(); // use a gap for each variable
    buffer_.resize(len + 1);
    WordID * const rend=&buffer_[0]+len;
    *rend=kNONE;
    WordID *r=rend;  // append by *--r = x
    const vector<WordID>& e = rule.e();
    //SRILM is reverse order null terminated
    //let's write down each phrase in reverse order and score it (note: we could lay them out consecutively then score them (we allocated enough buffer for that), but we won't actually use the whole buffer that way, since it wastes L1 cache.
    double sum=0.;
    for (unsigned j = 0; j < e.size(); ++j) {
      if (e[j] < 1) { // variable
        sum+=stateless_cost(r,rend);
        r=rend;
      } else { // terminal
          *--r=e[j];
      }
    }
    // last phrase (if any)
    return sum+stateless_cost(r,rend);
  }

  //NOTE: this is where the scoring of words happens (heuristic happens in EstimateProb)
  double LookupWords(const TRule& rule, const vector<const void*>& ant_states, void* vstate) {
    if (unigram)
      return stateless_cost(rule);
    int len = rule.ELength() - rule.Arity();
    for (int i = 0; i < ant_states.size(); ++i)
      len += StateSize(ant_states[i]);
    buffer_.resize(len + 1);
    buffer_[len] = kNONE;
    int i = len - 1;
    const vector<WordID>& e = rule.e();
    for (int j = 0; j < e.size(); ++j) {
      if (e[j] < 1) {
        const int* astate = reinterpret_cast<const int*>(ant_states[-e[j]]);
        int slen = StateSize(astate);
        for (int k = 0; k < slen; ++k)
          buffer_[i--] = astate[k];
      } else {
        buffer_[i--] = e[j];
      }
    }

    double sum = 0.0;
    int* remnant = reinterpret_cast<int*>(vstate);
    int j = 0;
    i = len - 1;
    int edge = len;

    while (i >= 0) {
      if (buffer_[i] == kSTAR) {
        edge = i;
      } else if (edge-i >= order_) {
        sum += LookupProbForBufferContents(i);
      } else if (edge == len && remnant) {
        remnant[j++] = buffer_[i];
      }
      --i;
    }
    if (!remnant) return sum;

    if (edge != len || len >= order_) {
      remnant[j++] = kSTAR;
      if (order_-1 < edge) edge = order_-1;
      for (int i = edge-1; i >= 0; --i)
        remnant[j++] = buffer_[i];
    }

    SetStateSize(j, vstate);
    return sum;
  }

private:
public:

 protected:
  vector<WordID> buffer_;
 public:
  WordID kSTART;
  WordID kSTOP;
  WordID kUNKNOWN;
  WordID kNONE;
  WordID kSTAR;
  bool unigram;
#endif

  lm::WordIndex MapWord(WordID w) const {
    if (w >= map_.size())
      return 0;
    else
      return map_[w];
  }

 public:
  KLanguageModelImpl(const std::string& param) {
    lm::ngram::Config conf;
    VMapper vm(&map_);
    conf.enumerate_vocab = &vm; 
    ngram_ = new lm::ngram::Model(param.c_str(), conf);
    cerr << "Loaded " << order_ << "-gram KLM from " << param << endl;
    order_ = ngram_->Order();
    state_size_ = ngram_->StateSize() + 1 + (order_-1) * sizeof(int);
  }

  ~KLanguageModelImpl() {
    delete ngram_;
  }

  const int ReserveStateSize() const { return state_size_; }

 private:
  lm::ngram::Model* ngram_;
  int order_;
  int state_size_;
  vector<lm::WordIndex> map_;

};

KLanguageModel::KLanguageModel(const string& param) {
  pimpl_ = new KLanguageModelImpl(param);
  fid_ = FD::Convert("LanguageModel");
  SetStateSize(pimpl_->ReserveStateSize());
}

Features KLanguageModel::features() const {
  return single_feature(fid_);
}

KLanguageModel::~KLanguageModel() {
  delete pimpl_;
}

void KLanguageModel::TraversalFeaturesImpl(const SentenceMetadata& /* smeta */,
                                          const Hypergraph::Edge& edge,
                                          const vector<const void*>& ant_states,
                                          SparseVector<double>* features,
                                          SparseVector<double>* estimated_features,
                                          void* state) const {
//  features->set_value(fid_, pimpl_->LookupWords(*edge.rule_, ant_states, state));
//  estimated_features->set_value(fid_, imp().EstimateProb(state));
}

void KLanguageModel::FinalTraversalFeatures(const void* ant_state,
                                           SparseVector<double>* features) const {
//  features->set_value(fid_, imp().FinalTraversalCost(ant_state));
}

