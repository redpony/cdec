#include "ff_csplit.h"

#include <set>
#include <cstring>

#include "Vocab.h"
#include "Ngram.h"

#include "sentence_metadata.h"
#include "lattice.h"
#include "tdict.h"
#include "freqdict.h"
#include "filelib.h"
#include "stringlib.h"
#include "tdict.h"

using namespace std;

struct BasicCSplitFeaturesImpl {
  BasicCSplitFeaturesImpl(const string& param) :
      word_count_(FD::Convert("WordCount")),
      letters_sq_(FD::Convert("LettersSq")),
      letters_sqrt_(FD::Convert("LettersSqrt")),
      in_dict_(FD::Convert("InDict")),
      short_(FD::Convert("Short")),
      long_(FD::Convert("Long")),
      oov_(FD::Convert("OOV")),
      short_range_(FD::Convert("ShortRange")),
      high_freq_(FD::Convert("HighFreq")),
      med_freq_(FD::Convert("MedFreq")),
      freq_(FD::Convert("Freq")),
      fl1_(FD::Convert("FreqLen1")),
      fl2_(FD::Convert("FreqLen2")),
      bad_(FD::Convert("Bad")) {
    vector<string> argv;
    int argc = SplitOnWhitespace(param, &argv);
    if (argc != 1 && argc != 2) {
      cerr << "Expected: freqdict.txt [badwords.txt]\n";
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
  }

  void TraversalFeaturesImpl(const Hypergraph::Edge& edge,
                             SparseVector<double>* features) const;

  const int word_count_;
  const int letters_sq_;
  const int letters_sqrt_;
  const int in_dict_;
  const int short_;
  const int long_;
  const int oov_;
  const int short_range_;
  const int high_freq_;
  const int med_freq_;
  const int freq_;
  const int fl1_;
  const int fl2_;
  const int bad_;
  FreqDict freq_dict_;
  set<WordID> bad_words_;
};

BasicCSplitFeatures::BasicCSplitFeatures(const string& param) :
  pimpl_(new BasicCSplitFeaturesImpl(param)) {}

void BasicCSplitFeaturesImpl::TraversalFeaturesImpl(
                                     const Hypergraph::Edge& edge,
                                     SparseVector<double>* features) const {
  features->set_value(word_count_, 1.0);
  features->set_value(letters_sq_, (edge.j_ - edge.i_) * (edge.j_ - edge.i_));
  features->set_value(letters_sqrt_, sqrt(edge.j_ - edge.i_));
  const WordID word = edge.rule_->e_[1];
  const char* sword = TD::Convert(word);
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
    features->set_value(freq_, freq);
    features->set_value(in_dict_, 1.0);
  } else {
    features->set_value(oov_, 1.0);
    freq = 99.0f;
  }
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
  pimpl_->TraversalFeaturesImpl(edge, features);
}

struct ReverseCharLMCSplitFeatureImpl {
  ReverseCharLMCSplitFeatureImpl(const string& param) :
      order_(5),
      vocab_(*TD::dict_),
      ngram_(vocab_, order_) {
    kBOS = vocab_.getIndex("<s>");
    kEOS = vocab_.getIndex("</s>");
    File file(param.c_str(), "r", 0);
    assert(file);
    cerr << "Reading " << order_ << "-gram LM from " << param << endl;
    ngram_.read(file);
  }

  double LeftPhonotacticProb(const Lattice& inword, const int start) {
    const int end = inword.size();
    for (int i = 0; i < order_; ++i)
      sc[i] = kBOS;
    int sp = min(end - start, order_ - 1);
    // cerr << "[" << start << "," << sp << "]\n";
    int ci = (order_ - sp - 1);
    int wi = start;
    while (sp > 0) {
      sc[ci] = inword[wi][0].label;
      // cerr << " CHAR: " << TD::Convert(sc[ci]) << "  ci=" << ci << endl;
      ++wi;
      ++ci;
      --sp;
    }
    // cerr << "  END ci=" << ci << endl;
    sc[ci] = Vocab_None;
    const double startprob = ngram_.wordProb(kEOS, sc);
    // cerr << "  PROB=" << startprob << endl;
    return startprob;
  }
 private:
  const int order_;
  Vocab& vocab_;
  VocabIndex kBOS;
  VocabIndex kEOS;
  Ngram ngram_;
  VocabIndex sc[80];
};

ReverseCharLMCSplitFeature::ReverseCharLMCSplitFeature(const string& param) :
  pimpl_(new ReverseCharLMCSplitFeatureImpl(param)),
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
#if 0
  WordID neighbor_word = 0;
  const WordID word = edge.rule_->e_[1];
  if (chars > 4 && (sword[0] == 's' || sword[0] == 'n')) {
    neighbor_word = TD::Convert(string(&sword[1]));
  }
  if (neighbor_word) {
    float nfreq = freq_dict_.LookUp(neighbor_word);
    cerr << "COMPARE: " << TD::Convert(word) << " & " << TD::Convert(neighbor_word) << endl;
    if (!nfreq) nfreq = 99.0f;
    features->set_value(fdoes_deletion_help_, (freq - nfreq));
  }
#endif
}

