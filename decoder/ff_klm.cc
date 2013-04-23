#include "ff_klm.h"

#include <cstring>
#include <cstdlib>
#include <iostream>

#include <boost/scoped_ptr.hpp>

#include "filelib.h"
#include "stringlib.h"
#include "hg.h"
#include "tdict.h"
#include "lm/model.hh"
#include "lm/enumerate_vocab.hh"
#include "utils/verbose.h"

#include "lm/left.hh"

using namespace std;

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

namespace {

struct VMapper : public lm::EnumerateVocab {
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

#pragma pack(push)
#pragma pack(1)

struct BoundaryAnnotatedState {
  lm::ngram::ChartState state;
  bool seen_bos, seen_eos;
};

#pragma pack(pop)

template <class Model> class BoundaryRuleScore {
  public:
    BoundaryRuleScore(const Model &m, BoundaryAnnotatedState &state) : 
        back_(m, state.state),
        bos_(state.seen_bos),
        eos_(state.seen_eos),
        penalty_(0.0),
        end_sentence_(m.GetVocabulary().EndSentence()) {
      bos_ = false;
      eos_ = false;
    }

    void BeginSentence() {
      back_.BeginSentence();
      bos_ = true;
    }

    void BeginNonTerminal(const BoundaryAnnotatedState &sub) {
      back_.BeginNonTerminal(sub.state, 0.0f);
      bos_ = sub.seen_bos;
      eos_ = sub.seen_eos;
    }

    void NonTerminal(const BoundaryAnnotatedState &sub) {
      back_.NonTerminal(sub.state, 0.0f);
      // cdec only calls this if there's content.  
      if (sub.seen_bos) {
        bos_ = true;
        penalty_ -= 100.0f;
      }
      if (eos_) penalty_ -= 100.0f;
      eos_ |= sub.seen_eos;
    }

    void Terminal(lm::WordIndex word) {
      back_.Terminal(word);
      if (eos_) penalty_ -= 100.0f;
      if (word == end_sentence_) eos_ = true;
    }

    float Finish() {
      return penalty_ + back_.Finish();
    }

  private:
    lm::ngram::RuleScore<Model> back_;
    bool &bos_, &eos_;

    float penalty_;

    lm::WordIndex end_sentence_;
};

} // namespace

template <class Model>
class KLanguageModelImpl {
 public:
  double LookupWords(const TRule& rule, const vector<const void*>& ant_states, double* oovs, double* emit, void* remnant) {
    *oovs = 0;
    *emit = 0;
    const vector<WordID>& e = rule.e();
    BoundaryRuleScore<Model> ruleScore(*ngram_, *static_cast<BoundaryAnnotatedState*>(remnant));
    unsigned i = 0;
    if (e.size()) {
      if (e[i] == kCDEC_SOS) {
        ++i;
        ruleScore.BeginSentence();
      } else if (e[i] <= 0) {  // special case for left-edge NT
        ruleScore.BeginNonTerminal(*static_cast<const BoundaryAnnotatedState*>(ant_states[-e[0]]));
        ++i;
      }
    }
    for (; i < e.size(); ++i) {
      if (e[i] <= 0) {
        ruleScore.NonTerminal(*static_cast<const BoundaryAnnotatedState*>(ant_states[-e[i]]));
      } else {
        float ep = 0.f;
        const WordID cdec_word_or_class = ClassifyWordIfNecessary(e[i], &ep);
        if (ep) { *emit += ep; }
        const lm::WordIndex cur_word = MapWord(cdec_word_or_class); // map to LM's id
        if (cur_word == 0) (*oovs) += 1.0;
        ruleScore.Terminal(cur_word);
      }
    }
    double ret = ruleScore.Finish();
    static_cast<BoundaryAnnotatedState*>(remnant)->state.ZeroRemaining();
    return ret;
  }

  // this assumes no target words on final unary -> goal rule.  is that ok?
  // for <s> (n-1 left words) and (n-1 right words) </s>
  double FinalTraversalCost(const void* state_void, double* oovs) {
    const BoundaryAnnotatedState &annotated = *static_cast<const BoundaryAnnotatedState*>(state_void);
    if (add_sos_eos_) {  // rules do not produce <s> </s>, so do it here
      assert(!annotated.seen_bos);
      assert(!annotated.seen_eos);
      lm::ngram::ChartState cstate;
      lm::ngram::RuleScore<Model> ruleScore(*ngram_, cstate);
      ruleScore.BeginSentence();
      ruleScore.NonTerminal(annotated.state, 0.0f);
      ruleScore.Terminal(kEOS_);
      return ruleScore.Finish();
    } else {  // rules DO produce <s> ... </s>
      double ret = 0.0;
      if (!annotated.seen_bos) ret -= 100.0;
      if (!annotated.seen_eos) ret -= 100.0;
      return ret;
    }
  }

  // if this is not a class-based LM, returns w untransformed,
  // otherwise returns a word class mapping of w,
  // returns TD::Convert("<unk>") if there is no mapping for w
  WordID ClassifyWordIfNecessary(WordID w, float* emitp) const {
    if (word2class_map_.empty()) return w;
    if (w >= word2class_map_.size())
      return kCDEC_UNK;
    else {
      *emitp = word2class_map_[w].second;
      return word2class_map_[w].first;
    }
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
      kCDEC_SOS(TD::Convert("<s>")) ,
      add_sos_eos_(!explicit_markers) {
    {
      VMapper vm(&cdec2klm_map_);
      lm::ngram::Config conf;
      conf.enumerate_vocab = &vm;
      ngram_ = new Model(filename.c_str(), conf);
    }
    order_ = ngram_->Order();
    if (!SILENT)
      cerr << "Loaded " << order_ << "-gram KLM from " << filename << " (MapSize=" << cdec2klm_map_.size() << ")\n";

    // special handling of beginning / ending sentence markers
    kSOS_ = MapWord(kCDEC_SOS);
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
    if (!SILENT)
      cerr << "  Loading word classes from " << file << " ...\n";
    AddWordToClassMapping_(TD::Convert("<s>"), TD::Convert("<s>"), 0.0);
    AddWordToClassMapping_(TD::Convert("</s>"), TD::Convert("</s>"), 0.0);
    while(getline(in, line)) {
      dummy.clear();
      TD::ConvertSentence(line, &dummy);
      ++lc;
      if (dummy.size() != 3) {
        cerr << "    Class map file expects: CLASS WORD logp(WORD|CLASS)\n";
        cerr << "    Format error in " << file << ", line " << lc << ": " << line << endl;
        abort();
      }
      AddWordToClassMapping_(dummy[1], dummy[0], strtof(TD::Convert(dummy[2]).c_str(), NULL));
    }
  }

  void AddWordToClassMapping_(WordID word, WordID cls, float emit) {
    if (word2class_map_.size() <= word) {
      word2class_map_.resize((word + 10) * 1.1, pair<WordID,float>(kCDEC_UNK,0.f));
      assert(word2class_map_.size() > word);
    }
    if(word2class_map_[word].first != kCDEC_UNK) {
      cerr << "Multiple classes for symbol " << TD::Convert(word) << endl;
      abort();
    }
    word2class_map_[word].first = cls;
    word2class_map_[word].second = emit;
  }

  ~KLanguageModelImpl() {
    delete ngram_;
  }

  int ReserveStateSize() const { return sizeof(BoundaryAnnotatedState); }

 private:
  const WordID kCDEC_UNK;
  const WordID kCDEC_SOS;
  lm::WordIndex kSOS_;  // <s> - requires special handling.
  lm::WordIndex kEOS_;  // </s>
  Model* ngram_;
  const bool add_sos_eos_; // flag indicating whether the hypergraph produces <s> and </s>
                     // if this is true, FinalTransitionFeatures will "add" <s> and </s>
                     // if false, FinalTransitionFeatures will score anything with the
                     // markers in the right place (i.e., the beginning and end of
                     // the sentence) with 0, and anything else with -100

  int order_;
  vector<lm::WordIndex> cdec2klm_map_;
  vector<pair<WordID,float> > word2class_map_; // if this is a class-based LM,
          // .first is the word->class mapping
          // .second is the emission log probability
};

template <class Model>
KLanguageModel<Model>::KLanguageModel(const string& param) {
  string filename, mapfile, featname;
  bool explicit_markers;
  if (!ParseLMArgs(param, &filename, &mapfile, &explicit_markers, &featname)) {
    abort();
  }
  try {
    pimpl_ = new KLanguageModelImpl<Model>(filename, mapfile, explicit_markers);
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
    abort();
  }
  fid_ = FD::Convert(featname);
  oov_fid_ = FD::Convert(featname+"_OOV");
  emit_fid_ = FD::Convert(featname+"_Emit");
  // cerr << "FID: " << oov_fid_ << endl;
  SetStateSize(pimpl_->ReserveStateSize());
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
  double emit = 0;
  features->set_value(fid_, pimpl_->LookupWords(*edge.rule_, ant_states, &oovs, &emit, state));
  if (oovs && oov_fid_)
    features->set_value(oov_fid_, oovs);
  if (emit && emit_fid_)
    features->set_value(emit_fid_, emit);
}

template <class Model>
void KLanguageModel<Model>::FinalTraversalFeatures(const void* ant_state,
                                           SparseVector<double>* features) const {
  double oovs = 0;
  double lm = pimpl_->FinalTraversalCost(ant_state, &oovs);
  features->set_value(fid_, lm);
  if (oov_fid_ && oovs)
    features->set_value(oov_fid_, oovs);
}

template <class Model> boost::shared_ptr<FeatureFunction> CreateModel(const std::string &param) {
  KLanguageModel<Model> *ret = new KLanguageModel<Model>(param);
  return boost::shared_ptr<FeatureFunction>(ret);
}

boost::shared_ptr<FeatureFunction> KLanguageModelFactory::Create(std::string param) const {
  using namespace lm::ngram;
  std::string filename, ignored_map;
  bool ignored_markers;
  std::string ignored_featname;
  ParseLMArgs(param, &filename, &ignored_map, &ignored_markers, &ignored_featname);
  ModelType m;
  if (!RecognizeBinary(filename.c_str(), m)) m = HASH_PROBING;

  switch (m) {
    case PROBING:
      return CreateModel<ProbingModel>(param);
    case REST_PROBING:
      return CreateModel<RestProbingModel>(param);
    case TRIE:
      return CreateModel<TrieModel>(param);
    case ARRAY_TRIE:
      return CreateModel<ArrayTrieModel>(param);
    case QUANT_TRIE:
      return CreateModel<QuantTrieModel>(param);
    case QUANT_ARRAY_TRIE:
      return CreateModel<QuantArrayTrieModel>(param);
    default:
      UTIL_THROW(util::Exception, "Unrecognized kenlm binary file type " << (unsigned)m);
  }
}

std::string  KLanguageModelFactory::usage(bool params,bool verbose) const {
  return KLanguageModel<lm::ngram::Model>::usage(params, verbose);
}

