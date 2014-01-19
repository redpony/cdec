#include "ff_csplit.h"

#include <set>
#include <cstring>

#include "klm/lm/model.hh"

#include "hg.h"
#include "sentence_metadata.h"
#include "lattice.h"
#include "tdict.h"
#include "freqdict.h"
#include "filelib.h"
#include "stringlib.h"
#include "tdict.h"

#ifndef HAVE_OLD_CPP
# include <unordered_set>
#else
# include <tr1/unordered_set>
namespace std { using std::tr1::unordered_set; }
#endif
using namespace std;

struct BasicCSplitFeaturesImpl {
  BasicCSplitFeaturesImpl(const string& param) :
      word_count_(FD::Convert("WordCount")),
      letters_sq_(FD::Convert("LettersSq")),
      letters_log_(FD::Convert("LettersLog")),
      letters_sqrt_(FD::Convert("LettersSqrt")),
      in_dict_(FD::Convert("InDict")),
      in_dict_sub_word_(FD::Convert("InDictSubWord")),
      short_(FD::Convert("Short")),
      long_(FD::Convert("Long")),
      oov_(FD::Convert("OOV")),
      oov_sub_word_(FD::Convert("OOVSubWord")),
      short_range_(FD::Convert("ShortRange")),
      high_freq_(FD::Convert("HighFreq")),
      med_freq_(FD::Convert("MedFreq")),
      logfreq_(FD::Convert("LogFreq")),
      loglogfreq_(FD::Convert("LogLogFreq")),
      in_dict_full_word_(FD::Convert("InDictFullWord")),
      fl1_(FD::Convert("FreqLen1")),
      fl2_(FD::Convert("FreqLen2")),
      bad_(FD::Convert("Bad")) {
    vector<string> argv;
    int argc = SplitOnWhitespace(param, &argv);
    if (argc != 1 && argc != 2 && argc != 3) {
      cerr << "Expected: freqdict.txt [badwords.txt] [sensitvewords.txt]\n";
      abort();
    }
    freq_dict_.Load(argv[0]);
    if (argc == 2) {
      ReadFile rf(argv[1]);
      istream& in = *rf.stream();
      while(in) {
        string badword;
        in >> badword;
        if (badword.empty()) continue;
        bad_words_.insert(TD::Convert(badword));
      }
    }
    if (argc == 3) {
      ReadFile rf(argv[2]);
      istream& in = *rf.stream();
      string line;
      while(getline(in, line)) {
        special_feats_[TD::Convert(line)] = FD::Convert("CS:"+line);
      }
    }
  }

  void TraversalFeaturesImpl(const Hypergraph::Edge& edge,
                             const int src_word_size,
                             SparseVector<double>* features) const;

  const int word_count_;
  const int letters_sq_;
  const int letters_log_;
  const int letters_sqrt_;
  const int in_dict_;
  const int in_dict_sub_word_;
  const int short_;
  const int long_;
  const int oov_;
  const int oov_sub_word_;
  const int short_range_;
  const int high_freq_;
  const int med_freq_;
  const int logfreq_;
  const int loglogfreq_;
  const int in_dict_full_word_;
  const int fl1_;
  const int fl2_;
  const int bad_;
  FreqDict<float> freq_dict_;
  set<WordID> bad_words_;
  unordered_map<WordID, int> special_feats_;
};

BasicCSplitFeatures::BasicCSplitFeatures(const string& param) :
  pimpl_(new BasicCSplitFeaturesImpl(param)) {}

void BasicCSplitFeaturesImpl::TraversalFeaturesImpl(
                                     const Hypergraph::Edge& edge,
                                     const int src_word_length,
                                     SparseVector<double>* features) const {
  const bool subword = (edge.i_ > 0) || (edge.j_ < src_word_length);
  string len_bias = "LenBias_0";
  int swlen = log(src_word_length) / log(1.69);
  if (swlen > 9) swlen = 9;
  len_bias[8] += swlen;
  int fid_len_bias_ = FD::Convert(len_bias);
  features->set_value(fid_len_bias_, 1.0); 
  features->set_value(word_count_, 1.0);
  features->set_value(letters_sq_, (edge.j_ - edge.i_) * (edge.j_ - edge.i_));
  features->set_value(letters_log_, log(edge.j_ - edge.i_));
  features->set_value(letters_sqrt_, sqrt(edge.j_ - edge.i_));
  const WordID word = edge.rule_->e_[1];
  const char* sword = TD::Convert(word).c_str();
  const int len = strlen(sword);
  int cur = 0;
  int chars = 0;
  while(cur < len) {
    cur += UTF8Len(sword[cur]);
    ++chars;
  }

  // these are corrections that attempt to make chars
  // more like a phoneme count than a letter count, they
  // are only really meaningful for german and should
  // probably be gotten rid of
  bool has_sch = strstr(sword, "sch");
  bool has_ch = (!has_sch && strstr(sword, "ch"));
  bool has_ie = strstr(sword, "ie");
  bool has_zw = strstr(sword, "zw");
  if (has_sch) chars -= 2;
  if (has_ch) --chars;
  if (has_ie) --chars;
  if (has_zw) --chars;

  float freq = freq_dict_.LookUp(word);
  if (freq) {
    features->set_value(logfreq_, freq);
    features->set_value(loglogfreq_, log(freq) / log(1.69));
    features->set_value(in_dict_, 1.0);
    if (subword) features->set_value(in_dict_sub_word_, 1.0);
  } else {
    if (!subword) features->set_value(in_dict_full_word_, 1.0);
    features->set_value(oov_, 1.0);
    if (subword) features->set_value(oov_sub_word_, 1.0);
    freq = 99.0f;
  }
  const unordered_map<WordID, int>::const_iterator it = special_feats_.find(word);
  if (it != special_feats_.end())
    features->set_value(it->second, 1.0);
  if (bad_words_.count(word) != 0)
    features->set_value(bad_, 1.0);
  if (chars < 5)
    features->set_value(short_, 1.0);
  if (chars > 10)
    features->set_value(long_, 1.0);
  if (freq < 7.0f)
    features->set_value(high_freq_, 1.0);
  if (freq > 8.0f && freq < 10.f)
    features->set_value(med_freq_, 1.0);
  if (freq < 10.0f && chars < 5)
    features->set_value(short_range_, 1.0);

  // i don't understand these features, but they really help!
  features->set_value(fl1_, sqrt(chars * freq));
  features->set_value(fl2_, freq / chars);
}

void BasicCSplitFeatures::PrepareForInput(const SentenceMetadata& smeta) {}

void BasicCSplitFeatures::TraversalFeaturesImpl(
                                     const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* out_context) const {
  (void) smeta;
  (void) ant_contexts;
  (void) out_context;
  (void) estimated_features;
  if (edge.Arity() == 0) return;
  if (edge.rule_->EWords() != 1) return;
  pimpl_->TraversalFeaturesImpl(edge, smeta.GetSourceLattice().size(), features);
}

namespace {
struct CSVMapper : public lm::EnumerateVocab {
  CSVMapper(vector<lm::WordIndex>* out) : out_(out), kLM_UNKNOWN_TOKEN(0) { out_->clear(); }
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

template<class Model>
struct ReverseCharLMCSplitFeatureImpl {
  ReverseCharLMCSplitFeatureImpl(const string& param) {
    CSVMapper vm(&cdec2klm_map_);
    lm::ngram::Config conf;
    conf.enumerate_vocab = &vm;
    cerr << "Reading character LM from " << param << endl;
    ngram_ = new Model(param.c_str(), conf);
    order_ = ngram_->Order();
    kEOS = MapWord(TD::Convert("</s>"));
    assert(kEOS > 0);
  }
  lm::WordIndex MapWord(const WordID w) const {
    if (w < cdec2klm_map_.size()) return cdec2klm_map_[w];
    return 0;
  }

  double LeftPhonotacticProb(const Lattice& inword, const int start) {
    const int end = inword.size();
    lm::ngram::State state = ngram_->BeginSentenceState();
    int sp = min(end - start, order_ - 1);
    // cerr << "[" << start << "," << sp << "]\n";
    int wi = start + sp - 1;
    while (sp > 0) {
      const lm::ngram::State scopy(state);
      ngram_->Score(scopy, MapWord(inword[wi][0].label), state);
      --wi;
      --sp;
    }
    const lm::ngram::State scopy(state);
    const double startprob = ngram_->Score(scopy, kEOS, state);
    return startprob;
  }
 private:
  Model* ngram_;
  int order_;
  vector<lm::WordIndex> cdec2klm_map_;
  lm::WordIndex kEOS;
};

ReverseCharLMCSplitFeature::ReverseCharLMCSplitFeature(const string& param) :
  pimpl_(new ReverseCharLMCSplitFeatureImpl<lm::ngram::ProbingModel>(param)),
  fid_(FD::Convert("RevCharLM")) {}

void ReverseCharLMCSplitFeature::TraversalFeaturesImpl(
                                     const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* out_context) const {
  (void) ant_contexts;
  (void) estimated_features;
  (void) out_context;

  if (edge.Arity() != 1) return;
  if (edge.rule_->EWords() != 1) return;
  const double lpp = pimpl_->LeftPhonotacticProb(smeta.GetSourceLattice(), edge.i_);
  features->set_value(fid_, lpp);
}

