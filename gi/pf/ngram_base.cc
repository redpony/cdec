#include "ngram_base.h"

#include "lm/model.hh"
#include "tdict.h"

using namespace std;

namespace {
struct GICSVMapper : public lm::EnumerateVocab {
  GICSVMapper(vector<lm::WordIndex>* out) : out_(out), kLM_UNKNOWN_TOKEN(0) { out_->clear(); }
  void Add(lm::WordIndex index, const StringPiece &str) {
    const WordID cdec_id = TD::Convert(str.as_string());
    if (cdec_id >= out_->size())
      out_->resize(cdec_id + 1, kLM_UNKNOWN_TOKEN);
    (*out_)[cdec_id] = index;
  }
  vector<lm::WordIndex>* out_;
  const lm::WordIndex kLM_UNKNOWN_TOKEN;
};
}

struct FixedNgramBaseImpl {
  FixedNgramBaseImpl(const string& param) {
    GICSVMapper vm(&cdec2klm_map_);
    lm::ngram::Config conf;
    conf.enumerate_vocab = &vm;
    cerr << "Reading character LM from " << param << endl;
    model = new lm::ngram::ProbingModel(param.c_str(), conf);
    order = model->Order();
    kEOS = MapWord(TD::Convert("</s>"));
    assert(kEOS > 0);
  }

  lm::WordIndex MapWord(const WordID w) const {
    if (w < cdec2klm_map_.size()) return cdec2klm_map_[w];
    return 0;
  }

  ~FixedNgramBaseImpl() { delete model; }

  prob_t StringProbability(const vector<WordID>& s) const {
    lm::ngram::State state = model->BeginSentenceState();
    double prob = 0;
    for (unsigned i = 0; i < s.size(); ++i) {
      const lm::ngram::State scopy(state);
      prob += model->Score(scopy, MapWord(s[i]), state);
    }
    const lm::ngram::State scopy(state);
    prob += model->Score(scopy, kEOS, state);
    prob_t p; p.logeq(prob * log(10));
    return p;
  }

  lm::ngram::ProbingModel* model;
  unsigned order;
  vector<lm::WordIndex> cdec2klm_map_;
  lm::WordIndex kEOS;
};

FixedNgramBase::~FixedNgramBase() { delete impl; }

FixedNgramBase::FixedNgramBase(const string& lmfname) {
  impl = new FixedNgramBaseImpl(lmfname);
}

prob_t FixedNgramBase::StringProbability(const vector<WordID>& s) const {
  return impl->StringProbability(s);
}

