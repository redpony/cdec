#include "ff_wordalign.h"

#include <algorithm>
#include <iterator>
#include <set>
#include <sstream>
#include <string>
#include <cmath>
#include <bitset>
#ifndef HAVE_OLD_CPP
# include <unordered_map>
#else
# include <tr1/unordered_map>
namespace std { using std::tr1::unordered_map; }
#endif

#include <boost/tuple/tuple.hpp>
#include "boost/tuple/tuple_comparison.hpp"
#include <boost/functional/hash.hpp>

#include "factored_lexicon_helper.h"
#include "verbose.h"
#include "stringlib.h"
#include "sentence_metadata.h"
#include "hg.h"
#include "fdict.h"
#include "aligner.h"
#include "tdict.h"   // Blunsom hack
#include "filelib.h" // Blunsom hack

static const int MAX_SENTENCE_SIZE = 100;

static const int kNULL_i = 255;  // -1 as an unsigned char

using namespace std;

// TODO new feature: if a word is translated as itself and there is a transition back to the same word, fire a feature

RelativeSentencePosition::RelativeSentencePosition(const string& param) :
    fid_(FD::Convert("RelativeSentencePosition")) {
  if (!param.empty()) {
    cerr << "  Loading word classes from " << param << endl;
    condition_on_fclass_ = true;
    ReadFile rf(param);
    istream& in = *rf.stream();
    set<WordID> classes;
    while(in) {
      string line;
      getline(in, line);
      if (line.empty()) continue;
      vector<WordID> v;
      TD::ConvertSentence(line, &v);
      pos_.push_back(v);
      for (int i = 0; i < v.size(); ++i)
        classes.insert(v[i]);
    }
    for (set<WordID>::iterator i = classes.begin(); i != classes.end(); ++i) {
      ostringstream os;
      os << "RelPos_FC:" << TD::Convert(*i);
      fids_[*i] = FD::Convert(os.str());
    }
  } else {
    condition_on_fclass_ = false;
  }
}

void RelativeSentencePosition::TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                                     const Hypergraph::Edge& edge,
                                                     const vector<const void*>& // ant_states
                                                     ,
                                                     SparseVector<double>* features,
                                                     SparseVector<double>* // estimated_features
                                                     ,
                                                     void* // state
  ) const {
  // if the source word is either null or the generated word
  // has no position in the reference
  if (edge.i_ == -1 || edge.prev_i_ == -1)
    return;

  assert(smeta.GetTargetLength() > 0);
  const double val = fabs(static_cast<double>(edge.i_) / smeta.GetSourceLength() -
                          static_cast<double>(edge.prev_i_) / smeta.GetTargetLength());
  features->set_value(fid_, val);
  if (condition_on_fclass_) {
    assert(smeta.GetSentenceID() < pos_.size());
    const WordID cur_fclass = pos_[smeta.GetSentenceID()][edge.i_];
    std::map<WordID, int>::const_iterator fidit = fids_.find(cur_fclass);
    assert(fidit != fids_.end());
    const int fid = fidit->second;
    features->set_value(fid, val);
  }
//  cerr << f_len_ << " " << e_len_ << " [" << edge.i_ << "," << edge.j_ << "|" << edge.prev_i_ << "," << edge.prev_j_ << "]\t" << edge.rule_->AsString() << "\tVAL=" << val << endl;
}

LexNullJump::LexNullJump(const string& param) :
    FeatureFunction(1),
    fid_lex_null_(FD::Convert("JumpLexNull")),
    fid_null_lex_(FD::Convert("JumpNullLex")),
    fid_null_null_(FD::Convert("JumpNullNull")),
    fid_lex_lex_(FD::Convert("JumpLexLex")) {}

void LexNullJump::TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                        const Hypergraph::Edge& edge,
                                        const vector<const void*>& ant_states,
                                        SparseVector<double>* features,
                                        SparseVector<double>* /* estimated_features */,
                                        void* state) const {
  char& dpstate = *((char*)state);
  if (edge.Arity() == 0) {
    // dpstate is 'N' = null or 'L' = lex
    if (edge.i_ < 0) { dpstate = 'N'; } else { dpstate = 'L'; }
  } else if (edge.Arity() == 1) {
    dpstate = *((unsigned char*)ant_states[0]);
  } else if (edge.Arity() == 2) {
    char left = *((char*)ant_states[0]);
    char right = *((char*)ant_states[1]);
    dpstate = right;
    if (left == 'N') {
      if (right == 'N')
        features->set_value(fid_null_null_, 1.0);
      else
        features->set_value(fid_null_lex_, 1.0);
    } else { // left == 'L'
      if (right == 'N')
        features->set_value(fid_lex_null_, 1.0);
      else
        features->set_value(fid_lex_lex_, 1.0);
    }
  } else {
    assert(!"something really unexpected is happening");
  }
}

NewJump::NewJump(const string& param) :
    FeatureFunction(1),
    kBOS_(TD::Convert("BOS")),
    kEOS_(TD::Convert("EOS")) {
  cerr << "    NewJump";
  vector<string> argv;
  set<string> permitted;
  permitted.insert("use_binned_log_lengths");
  permitted.insert("flen");
  permitted.insert("elen");
  permitted.insert("fprev");
  permitted.insert("f0");
  permitted.insert("f-1");
  permitted.insert("f+1");
  // also permitted f:FILENAME
  int argc = SplitOnWhitespace(param, &argv);
  set<string> config;
  string f_file;
  for (int i = 0; i < argc; ++i) {
    if (argv[i].size() > 2 && argv[i].find("f:") == 0) {
      assert(f_file.empty());  // only one f file!
      f_file = argv[i].substr(2);
      cerr << " source_file=" << f_file;
    } else {
      if (permitted.count(argv[i])) {
        assert(config.count(argv[i]) == 0);
        config.insert(argv[i]);
        cerr << " " << argv[i];
      } else {
        cerr << "\nNewJump: don't understand param '" << argv[i] << "'\n";
        abort();
      }
    }
  }
  cerr << endl;
  use_binned_log_lengths_ = config.count("use_binned_log_lengths") > 0;
  f0_ = config.count("f0") > 0;
  fm1_ = config.count("f-1") > 0;
  fp1_ = config.count("f+1") > 0;
  fprev_ = config.count("fprev") > 0;
  elen_ = config.count("elen") > 0;
  flen_ = config.count("flen") > 0;
  if (f0_ || fm1_ || fp1_ || fprev_) {
    if (f_file.empty()) {
      cerr << "NewJump: conditioning on src but f:FILE not specified!\n";
      abort();
    }
    ReadFile rf(f_file);
    istream& in = *rf.stream();
    string line;
    while(in) {
      getline(in, line);
      if (!in) continue;
      vector<WordID> v;
      TD::ConvertSentence(line, &v);
      src_.push_back(v);
    }
  }
  fid_str_ = "J";
  if (flen_) fid_str_ += "F";
  if (elen_) fid_str_ += "E";
  if (f0_) fid_str_ += "C";
  if (fm1_) fid_str_ += "L";
  if (fp1_) fid_str_ += "R";
  if (fprev_) fid_str_ += "P";
}

// do a log transform on the length (of a sentence, a jump, etc)
// this basically means that large distances that are close to each other
// are put into the same bin
int BinnedLogLength(int len) {
  int res = static_cast<int>(log(len+1) / log(1.3));
  if (res > 16) res = 16;
  return res;
}

// <0>=jump size <1>=jump_dir <2>=flen, <3>=elen, <4>=f0, <5>=f-1, <6>=f+1, <7>=fprev
typedef boost::tuple<short, char, short, short, WordID, WordID, WordID, WordID> NewJumpFeatureKey;

struct KeyHash : unary_function<NewJumpFeatureKey, size_t> {
  size_t operator()(const NewJumpFeatureKey& k) const {
    size_t h = 0x37473DEF321;
    boost::hash_combine(h, k.get<0>());
    boost::hash_combine(h, k.get<1>());
    boost::hash_combine(h, k.get<2>());
    boost::hash_combine(h, k.get<3>());
    boost::hash_combine(h, k.get<4>());
    boost::hash_combine(h, k.get<5>());
    boost::hash_combine(h, k.get<6>());
    boost::hash_combine(h, k.get<7>());
    return h;
  }
};

void NewJump::FireFeature(const SentenceMetadata& smeta,
                          const int prev_src_index,
                          const int cur_src_index,
                          SparseVector<double>* features) const {
  const int id = smeta.GetSentenceID();
  const int src_len = smeta.GetSourceLength();
  const int raw_jump = cur_src_index - prev_src_index;
  short jump_magnitude = raw_jump;
  char jtype = 0;
  if (raw_jump > 0) { jtype = 'R'; } // Right
  else if (raw_jump == 0) { jtype = 'S'; } // Stay
  else { jtype = 'L'; jump_magnitude = raw_jump * -1; } // Left
  int effective_src_len = src_len;
  int effective_trg_len = smeta.GetTargetLength();
  if (use_binned_log_lengths_) {
    jump_magnitude = BinnedLogLength(jump_magnitude);
    effective_src_len = BinnedLogLength(src_len);
    effective_trg_len = BinnedLogLength(effective_trg_len);
  }
  NewJumpFeatureKey key(jump_magnitude,jtype,0,0,0,0,0);
  using boost::get;
  if (flen_)  get<2>(key) = effective_src_len;
  if (elen_)  get<3>(key) = effective_trg_len;
  if (f0_)    get<4>(key) = GetSourceWord(id, cur_src_index);
  if (fm1_)   get<5>(key) = GetSourceWord(id, cur_src_index - 1);
  if (fp1_)   get<6>(key) = GetSourceWord(id, cur_src_index + 1);
  if (fprev_) get<7>(key) = GetSourceWord(id, prev_src_index);

  static std::unordered_map<NewJumpFeatureKey, int, KeyHash> fids;
  int& fid = fids[key];
  if (!fid) {
    ostringstream os;
    os << fid_str_ << ':' << jtype << jump_magnitude;
    if (flen_)  os << ':' << get<2>(key);
    if (elen_)  os << ':' << get<3>(key);
    if (f0_)    os << ':' << TD::Convert(get<4>(key));
    if (fm1_)   os << ':' << TD::Convert(get<5>(key));
    if (fp1_)   os << ':' << TD::Convert(get<6>(key));
    if (fprev_) os << ':' << TD::Convert(get<7>(key));    
    fid = FD::Convert(os.str());
  }
  features->set_value(fid, 1.0);
}

void NewJump::TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                       const Hypergraph::Edge& edge,
                                       const vector<const void*>& ant_states,
                                       SparseVector<double>* features,
                                       SparseVector<double>* /* estimated_features */,
                                       void* state) const {
  unsigned char& dpstate = *((unsigned char*)state);
  // IMPORTANT: this only fires on non-Null transitions!
  const int flen = smeta.GetSourceLength();
  if (edge.Arity() == 0) {
    dpstate = static_cast<unsigned int>(edge.i_);
    if (edge.prev_i_ == 0) {     // first target word in sentence
      if (edge.i_ >= 0) {   // generated from non-Null token?
        FireFeature(smeta,
                    -1,  // previous src = beginning of sentence index
                    edge.i_, // current src
                    features);
      }
    } else if (edge.prev_i_ == smeta.GetTargetLength() - 1) {  // last word
      if (edge.i_ >= 0) {  // generated from non-Null token?
        FireFeature(smeta,
                    edge.i_,  // previous src = last word position
                    flen,     // current src
                    features);
      }
    }
  } else if (edge.Arity() == 1) {
    dpstate = *((unsigned char*)ant_states[0]);
  } else if (edge.Arity() == 2) {
    int left_index = *((unsigned char*)ant_states[0]);
    int right_index = *((unsigned char*)ant_states[1]);
    if (right_index == -1)
      dpstate = static_cast<unsigned int>(left_index);
    else
      dpstate = static_cast<unsigned int>(right_index);
    if (left_index != kNULL_i && right_index != kNULL_i) {
      FireFeature(smeta,
                  left_index,          // previous src index
                  right_index,         // current src index
                  features);
    }
  } else {
    assert(!"something really unexpected is happening");
  }
}

SourceBigram::SourceBigram(const std::string& param) :
    FeatureFunction(sizeof(WordID) + sizeof(int)) {
  fid_str_ = "SB:";
  if (param.size() > 0) {
    vector<string> argv;
    int argc = SplitOnWhitespace(param, &argv);
    if (argc != 2) {
      cerr << "SourceBigram [FEATURE_NAME_PREFIX PATH]\n";
      abort();
    }
    fid_str_ = argv[0] + ":";
    lexmap_.reset(new FactoredLexiconHelper(argv[1], "*"));
  } else {
    lexmap_.reset(new FactoredLexiconHelper);
  }
}

void SourceBigram::PrepareForInput(const SentenceMetadata& smeta) {
  lexmap_->PrepareForInput(smeta);
}

void SourceBigram::FinalTraversalFeatures(const void* context,
                                      SparseVector<double>* features) const {
  WordID left = *static_cast<const WordID*>(context);
  int left_wc = *(static_cast<const int*>(context) + 1);
  if (left_wc == 1)
    FireFeature(-1, left, features);
  FireFeature(left, -1, features);
}

void SourceBigram::FireFeature(WordID left,
                   WordID right,
                   SparseVector<double>* features) const {
  int& fid = fmap_[left][right];
  // TODO important important !!! escape strings !!!
  if (!fid) {
    ostringstream os;
    os << fid_str_;
    if (left < 0) { os << "BOS"; } else { os << TD::Convert(left); }
    os << '_';
    if (right < 0) { os << "EOS"; } else { os << TD::Convert(right); }
    fid = FD::Convert(os.str());
    if (fid == 0) fid = -1;
  }
  if (fid > 0) features->set_value(fid, 1.0);
}

void SourceBigram::TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                            SparseVector<double>* /* estimated_features */,
                                     void* context) const {
  WordID& out_context = *static_cast<WordID*>(context);
  int& out_word_count = *(static_cast<int*>(context) + 1);
  const int arity = edge.Arity();
  if (arity == 0) {
    out_context = lexmap_->SourceWordAtPosition(edge.i_);
    out_word_count = edge.rule_->EWords();
    assert(out_word_count == 1); // this is only defined for lex translation!
    // revisit this if you want to translate into null words
  } else if (arity == 1) {
    WordID left = *static_cast<const WordID*>(ant_contexts[0]);
    int left_wc = *(static_cast<const int*>(ant_contexts[0]) + 1);
    out_context = left;
    out_word_count = left_wc;
  } else if (arity == 2) {
    WordID left = *static_cast<const WordID*>(ant_contexts[0]);
    WordID right = *static_cast<const WordID*>(ant_contexts[1]);
    int left_wc = *(static_cast<const int*>(ant_contexts[0]) + 1);
    int right_wc = *(static_cast<const int*>(ant_contexts[0]) + 1);
    if (left_wc == 1 && right_wc == 1)
      FireFeature(-1, left, features);
    FireFeature(left, right, features);
    out_word_count = left_wc + right_wc;
    out_context = right;
  }
}

LexicalTranslationTrigger::LexicalTranslationTrigger(const std::string& param) :
    FeatureFunction(0) {
  if (param.empty()) {
    cerr << "LexicalTranslationTrigger requires a parameter (file containing triggers)!\n";
  } else {
    ReadFile rf(param);
    istream& in = *rf.stream();
    string line;
    while(in) {
      getline(in, line);
      if (!in) continue;
      vector<WordID> v;
      TD::ConvertSentence(line, &v);
      triggers_.push_back(v);
    }
  }
}
  
void LexicalTranslationTrigger::FireFeature(WordID trigger, 
                                     WordID src,
                                     WordID trg,
                                     SparseVector<double>* features) const {
  int& fid = fmap_[trigger][src][trg];
  if (!fid) {
    ostringstream os;
    os << "T:" << TD::Convert(trigger) << ':' << TD::Convert(src) << '_' << TD::Convert(trg);
    fid = FD::Convert(os.str());
  }
  features->set_value(fid, 1.0);

  int &tfid = target_fmap_[trigger][trg];
  if (!tfid) {
    ostringstream os;
    os << "TT:" << TD::Convert(trigger) << ':' << TD::Convert(trg);
    tfid = FD::Convert(os.str());
  }
  features->set_value(tfid, 1.0);
}

void LexicalTranslationTrigger::TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                            SparseVector<double>* /* estimated_features */,
                                     void* context) const {
  if (edge.Arity() == 0) {
    assert(edge.rule_->EWords() == 1);
    assert(edge.rule_->FWords() == 1);
    WordID trg = edge.rule_->e()[0]; 
    WordID src = edge.rule_->f()[0];
    assert(triggers_.size() > smeta.GetSentenceID());
    const vector<WordID>& triggers = triggers_[smeta.GetSentenceID()];
    for (int i = 0; i < triggers.size(); ++i) {
      FireFeature(triggers[i], src, trg, features);
    }
  }
}

BlunsomSynchronousParseHack::BlunsomSynchronousParseHack(const string& param) :
  FeatureFunction((100 / 8) + 1), fid_(FD::Convert("NotRef")), cur_sent_(-1) {
  ReadFile rf(param);
  istream& in = *rf.stream(); int lc = 0;
  while(in) {
    string line;
    getline(in, line);
    if (!in) break;
    ++lc;
    refs_.push_back(vector<WordID>());
    TD::ConvertSentence(line, &refs_.back());
  }
  cerr << "  Loaded " << lc << " refs\n";
}

void BlunsomSynchronousParseHack::TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                           const Hypergraph::Edge& edge,
                                           const vector<const void*>& ant_states,
                                           SparseVector<double>* features,
                                           SparseVector<double>* /* estimated_features */,
                                           void* state) const {
  if (cur_sent_ != smeta.GetSentenceID()) {
    // assert(smeta.HasReference());
    cur_sent_ = smeta.GetSentenceID();
    assert(cur_sent_ < refs_.size());
    cur_ref_ = &refs_[cur_sent_];
    cur_map_.clear();
    for (int i = 0; i < cur_ref_->size(); ++i) {
      vector<WordID> phrase;
      for (int j = i; j < cur_ref_->size(); ++j) {
        phrase.push_back((*cur_ref_)[j]);
        cur_map_[phrase] = i;
      }
    }
  }
  //cerr << edge.rule_->AsString() << endl;
  for (int i = 0; i < ant_states.size(); ++i) {
    if (DoesNotBelong(ant_states[i])) {
      //cerr << "  ant " << i << " does not belong\n";
      return;
    }
  }
  vector<vector<WordID> > ants(ant_states.size());
  vector<const vector<WordID>* > pants(ant_states.size());
  for (int i = 0; i < ant_states.size(); ++i) {
    AppendAntecedentString(ant_states[i], &ants[i]);
    //cerr << "  ant[" << i << "]: " << ((int)*(static_cast<const unsigned char*>(ant_states[i]))) << " " << TD::GetString(ants[i]) << endl;
    pants[i] = &ants[i];
  }
  vector<WordID> yield;
  edge.rule_->ESubstitute(pants, &yield);
  //cerr << "YIELD: " << TD::GetString(yield) << endl;
  Vec2Int::iterator it = cur_map_.find(yield);
  if (it == cur_map_.end()) {
    features->set_value(fid_, 1);
    //cerr << "  BAD!\n";
    return;
  }
  SetStateMask(it->second, it->second + yield.size(), state);
}

IdentityCycleDetector::IdentityCycleDetector(const std::string& param) : FeatureFunction(2) {
  length_min_ = 3;
  if (!param.empty())
    length_min_ = atoi(param.c_str());
  assert(length_min_ >= 0);
  ostringstream os;
  os << "IdentityCycle_LenGT" << length_min_;
  fid_ = FD::Convert(os.str());
}

inline bool IsIdentityTranslation(const void* state) {
  return static_cast<const unsigned char*>(state)[0];
}

inline int SourceIndex(const void* state) {
  unsigned char i = static_cast<const unsigned char*>(state)[1];
  if (i == 255) return -1;
  return i;
}

void IdentityCycleDetector::TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const {
  unsigned char* out_state = static_cast<unsigned char*>(context);
  unsigned char& out_is_identity = out_state[0];
  unsigned char& out_src_index = out_state[1];

  if (edge.Arity() == 0) {
    assert(edge.rule_->EWords() == 1);
    assert(edge.rule_->FWords() == 1);
    out_src_index = edge.i_;
    out_is_identity = false;
    if (edge.rule_->e_[0] == edge.rule_->f_[0]) {
      const WordID word = edge.rule_->e_[0];
      static map<WordID, bool> big_enough;
      map<WordID,bool>::iterator it = big_enough_.find(word);
      if (it == big_enough_.end()) {
        out_is_identity = big_enough_[word] = TD::Convert(word).size() >= length_min_;
      } else {
        out_is_identity = it->second;
      }
    }
  } else if (edge.Arity() == 1) {
    memcpy(context, ant_contexts[0], 2);
  } else if (edge.Arity() == 2) {
    bool left_identity = IsIdentityTranslation(ant_contexts[0]);
    int left_index = SourceIndex(ant_contexts[0]);
    bool right_identity = IsIdentityTranslation(ant_contexts[1]);
    int right_index = SourceIndex(ant_contexts[1]);
    if ((left_identity && left_index == right_index && !right_identity) ||
        (right_identity && left_index == right_index && !left_identity)) {
      features->set_value(fid_, 1.0);
    }
    out_is_identity = right_identity;
    out_src_index = right_index;
  } else { assert("really really bad"); }
}


InputIndicator::InputIndicator(const std::string& param) {}

void InputIndicator::FireFeature(WordID src,
                                 SparseVector<double>* features) const {
  int& fid = fmap_[src];
  if (!fid) {
    static map<WordID, WordID> escape;
    if (escape.empty()) {
      escape[TD::Convert("=")] = TD::Convert("__EQ");
      escape[TD::Convert(";")] = TD::Convert("__SC");
      escape[TD::Convert(",")] = TD::Convert("__CO");
    }
    if (escape.count(src)) src = escape[src];
    ostringstream os;
    os << "S:" << TD::Convert(src);
    fid = FD::Convert(os.str());
  }
  features->set_value(fid, 1.0);
}

void InputIndicator::TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const {
  const vector<WordID>& fw = edge.rule_->f_;
  for (int i = 0; i < fw.size(); ++i) {
    const WordID& f = fw[i];
    if (f > 0) FireFeature(f, features);
  }
}

WordPairFeatures::WordPairFeatures(const string& param) {
  vector<string> argv;
  int argc = SplitOnWhitespace(param, &argv); 
  if (argc != 1) {
    cerr << "WordPairFeature /path/to/feature_values.table\n";
    abort();
  }
  set<WordID> all_srcs;
  {
    ReadFile rf(argv[0]);
    istream& in = *rf.stream();
    string buf;
    while (in) {
      getline(in, buf);
      if (buf.empty()) continue;
      int start = 0;
      while(start < buf.size() && buf[start] == ' ') ++start;
      int end = start;
      while(end < buf.size() && buf[end] != ' ') ++end;
      const WordID src = TD::Convert(buf.substr(start, end - start));
      all_srcs.insert(src);
    }
  }
  if (all_srcs.empty()) {
    cerr << "WordPairFeature " << param << " loaded empty file!\n";
    return;
  }
  fkeys_.reserve(all_srcs.size());
  copy(all_srcs.begin(), all_srcs.end(), back_inserter(fkeys_));
  values_.resize(all_srcs.size());
  if (!SILENT) { cerr << "WordPairFeature: " << all_srcs.size() << " sources\n"; }
  ReadFile rf(argv[0]);
  istream& in = *rf.stream();
  string buf;
  double val = 0;
  WordID cur_src = 0;
  map<WordID, SparseVector<float> > *pv = NULL;
  const WordID kBARRIER = TD::Convert("|||");
  while (in) {
    getline(in, buf);
    if (buf.size() == 0) continue;
    int start = 0;
    while(start < buf.size() && buf[start] == ' ') ++start;
    int end = start;
    while(end < buf.size() && buf[end] != ' ') ++end;
    const WordID src = TD::Convert(buf.substr(start, end - start));
    if (cur_src != src) {
      cur_src = src;
      size_t ind = distance(fkeys_.begin(), lower_bound(fkeys_.begin(), fkeys_.end(), cur_src));
      pv = &values_[ind];
    }
    end += 1;
    start = end;
    while(end < buf.size() && buf[end] != ' ') ++end;
    WordID x = TD::Convert(buf.substr(start, end - start));
    if (x != kBARRIER) {
      cerr << "1 Format error: " << buf << endl;
      abort();
    }
    start = end + 1;
    end = start + 1;
    while(end < buf.size() && buf[end] != ' ') ++end;
    WordID trg = TD::Convert(buf.substr(start, end - start));
    if (trg == kBARRIER) {
      cerr << "2 Format error: " << buf << endl;
      abort();
    }
    start = end + 1;
    end = start + 1;
    while(end < buf.size() && buf[end] != ' ') ++end;
    WordID x2 = TD::Convert(buf.substr(start, end - start));
    if (x2 != kBARRIER) {
      cerr << "3 Format error: " << buf << endl;
      abort();
    }
    start = end + 1;

    SparseVector<float>& v = (*pv)[trg];
    while(start < buf.size()) {
      end = start + 1;
      while(end < buf.size() && buf[end] != '=' && buf[end] != ' ') ++end;
      if (end == buf.size() || buf[end] != '=') { cerr << "4 Format error: " << buf << endl; abort(); }
      const int fid = FD::Convert(buf.substr(start, end - start));
      start = end + 1;
      while(start < buf.size() && buf[start] == ' ') ++start;
      end = start + 1;
      while(end < buf.size() && buf[end] != ' ') ++end;
      assert(end > start);
      if (end < buf.size()) buf[end] = 0;
      val = strtod(&buf.c_str()[start], NULL);
      v.set_value(fid, val);
      start = end + 1;
    }
  }
}

void WordPairFeatures::TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const {
  if (edge.Arity() == 0) {
    assert(edge.rule_->EWords() == 1);
    assert(edge.rule_->FWords() == 1);
    const WordID trg = edge.rule_->e()[0]; 
    const WordID src = edge.rule_->f()[0];
    size_t ind = distance(fkeys_.begin(), lower_bound(fkeys_.begin(), fkeys_.end(), src));
    if (ind == fkeys_.size() || fkeys_[ind] != src) {
      cerr << "WordPairFeatures no source entries for " << TD::Convert(src) << endl;
      abort();
    }
    const map<WordID, SparseVector<float> >::const_iterator it = values_[ind].find(trg);
    // TODO optional strict flag to make sure there are features for all pairs?
    if (it != values_[ind].end())
      (*features) += it->second;
  }
}

struct PathFertility {
  unsigned char null_fertility;
  unsigned char index_fertility[255];
  PathFertility& operator+=(const PathFertility& rhs) {
    null_fertility += rhs.null_fertility;
    for (int i = 0; i < 255; ++i)
      index_fertility[i] += rhs.index_fertility[i];
    return *this;
  }
};

Fertility::Fertility(const string& config) :
    FeatureFunction(sizeof(PathFertility)) {}

void Fertility::TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                      const Hypergraph::Edge& edge,
                                      const std::vector<const void*>& ant_contexts,
                                      SparseVector<double>* features,
                                      SparseVector<double>* estimated_features,
                                      void* context) const {
  PathFertility& out_fert = *static_cast<PathFertility*>(context);
  if (edge.Arity() == 0) {
    if (edge.i_ < 0) {
      out_fert.null_fertility = 1;
    } else {
      out_fert.index_fertility[edge.i_] = 1;
    }
  } else if (edge.Arity() == 2) {
    const PathFertility left = *static_cast<const PathFertility*>(ant_contexts[0]);
    const PathFertility right = *static_cast<const PathFertility*>(ant_contexts[1]);
    out_fert += left;
    out_fert += right;
  } else if (edge.Arity() == 1) {
    out_fert += *static_cast<const PathFertility*>(ant_contexts[0]);
  }
}

