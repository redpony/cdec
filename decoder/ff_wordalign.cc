#include "ff_wordalign.h"

#include <algorithm>
#include <iterator>
#include <set>
#include <sstream>
#include <string>
#include <cmath>

#include "verbose.h"
#include "alignment_pharaoh.h"
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

Model2BinaryFeatures::Model2BinaryFeatures(const string& ) :
    fids_(boost::extents[MAX_SENTENCE_SIZE][MAX_SENTENCE_SIZE][MAX_SENTENCE_SIZE]) {
  for (int i = 1; i < MAX_SENTENCE_SIZE; ++i) {
    for (int j = 0; j < i; ++j) {
      for (int k = 0; k < MAX_SENTENCE_SIZE; ++k) {
        int& val = fids_[i][j][k];
        val = -1;
        if (j < i) {
          ostringstream os;
          os << "M2FL:" << i << ":TI:" << k << "_SI:" << j;
          val = FD::Convert(os.str());
        }
      }
    }
  }
}

void Model2BinaryFeatures::TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                                 const Hypergraph::Edge& edge,
                                                 const vector<const void*>& /*ant_states*/,
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
  const int fid = fids_[smeta.GetSourceLength()][edge.i_][edge.prev_i_];
  features->set_value(fid, 1.0);
//  cerr << f_len_ << " " << e_len_ << " [" << edge.i_ << "," << edge.j_ << "|" << edge.prev_i_ << "," << edge.prev_j_ << "]\t" << edge.rule_->AsString() << "\tVAL=" << val << endl;
}


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

MarkovJumpFClass::MarkovJumpFClass(const string& param) :
    FeatureFunction(1),
    fids_(MAX_SENTENCE_SIZE) {
  cerr << "    MarkovJumpFClass" << endl;
  cerr << "Reading source POS tags from " << param << endl;
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
  cerr << "  (" << pos_.size() << " lines)\n";
  cerr << "  Classes: " << classes.size() << endl;
  for (int ss = 1; ss < MAX_SENTENCE_SIZE; ++ss) {
    map<WordID, map<int, int> >& cfids = fids_[ss];
    for (set<WordID>::iterator i = classes.begin(); i != classes.end(); ++i) {
      map<int, int> &fids = cfids[*i];
      for (int j = -ss; j <= ss; ++j) {
        ostringstream os;
        os << "Jump_FL:" << ss << "_FC:" << TD::Convert(*i) << "_J:" << j;
        fids[j] = FD::Convert(os.str());
      }
    }
  }
}

void MarkovJumpFClass::FireFeature(const SentenceMetadata& smeta,
                                   int prev_src_pos,
                                   int cur_src_pos,
                                   SparseVector<double>* features) const {
  if (prev_src_pos == kNULL_i || cur_src_pos == kNULL_i)
    return;

  const int jumpsize = cur_src_pos - prev_src_pos;

  assert(smeta.GetSentenceID() < pos_.size());
  const WordID cur_fclass = pos_[smeta.GetSentenceID()][cur_src_pos];
  const int fid = fids_[smeta.GetSourceLength()].find(cur_fclass)->second.find(jumpsize)->second;
  features->set_value(fid, 1.0);
}

void MarkovJumpFClass::FinalTraversalFeatures(const void* context,
                                      SparseVector<double>* features) const {
  int left_index = *static_cast<const unsigned char*>(context);
//  int right_index = cur_flen;
  // TODO
}

void MarkovJumpFClass::TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_states,
                                     SparseVector<double>* features,
                                     SparseVector<double>* /* estimated_features */,
                                     void* state) const {
  unsigned char& dpstate = *((unsigned char*)state);
  if (edge.Arity() == 0) {
    dpstate = static_cast<unsigned int>(edge.i_);
  } else if (edge.Arity() == 1) {
    dpstate = *((unsigned char*)ant_states[0]);
  } else if (edge.Arity() == 2) {
    int left_index = *((unsigned char*)ant_states[0]);
    int right_index = *((unsigned char*)ant_states[1]);
    if (right_index == -1)
      dpstate = static_cast<unsigned int>(left_index);
    else
      dpstate = static_cast<unsigned int>(right_index);
//    const WordID cur_fclass = pos_[smeta.GetSentenceID()][right_index];
//    cerr << edge.i_ << "," << edge.j_ << ": fclass=" << TD::Convert(cur_fclass) << " j=" << jumpsize << endl;
//    const int fid = fids_[smeta.GetSourceLength()].find(cur_fclass)->second.find(jumpsize)->second;
//    features->set_value(fid, 1.0);
    FireFeature(smeta, left_index, right_index, features);
  }
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

MarkovJump::MarkovJump(const string& param) :
    FeatureFunction(1),
    fid_(FD::Convert("MarkovJump")),
    fid_lex_null_(FD::Convert("JumpLexNull")),
    fid_null_lex_(FD::Convert("JumpNullLex")),
    fid_null_null_(FD::Convert("JumpNullNull")),
    fid_lex_lex_(FD::Convert("JumpLexLex")),
    binary_params_(false) {
  cerr << "    MarkovJump";
  vector<string> argv;
  int argc = SplitOnWhitespace(param, &argv);
  if (argc != 1 || !(argv[0] == "-b" || argv[0] == "+b")) {
    cerr << "MarkovJump: expected parameters to be -b or +b\n";
    exit(1);
  }
  binary_params_ = argv[0] == "+b";
  if (binary_params_) {
    flen2jump2fid_.resize(MAX_SENTENCE_SIZE);
    for (int i = 1; i < MAX_SENTENCE_SIZE; ++i) {
      map<int, int>& jump2fid = flen2jump2fid_[i];
      for (int jump = -i; jump <= i; ++jump) {
        ostringstream os;
        os << "Jump:FLen:" << i << "_J:" << jump;
        jump2fid[jump] = FD::Convert(os.str());
      }
    }
  } else {
    cerr << " (Blunsom & Cohn definition)";
  }
  cerr << endl;
}

// TODO handle NULLs according to Och 2000?
void MarkovJump::TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                       const Hypergraph::Edge& edge,
                                       const vector<const void*>& ant_states,
                                       SparseVector<double>* features,
                                       SparseVector<double>* /* estimated_features */,
                                       void* state) const {
  unsigned char& dpstate = *((unsigned char*)state);
  const int flen = smeta.GetSourceLength();
  if (edge.Arity() == 0) {
    dpstate = static_cast<unsigned int>(edge.i_);
    if (edge.prev_i_ == 0) {     // first word in sentence
      if (edge.i_ >= 0 && binary_params_) {
        const int fid = flen2jump2fid_[flen].find(edge.i_ + 1)->second;
        features->set_value(fid, 1.0);
      } else if (edge.i_ < 0 && binary_params_) {
        // handled by bigram features
      }
    } else if (edge.prev_i_ == smeta.GetTargetLength() - 1) {
      if (edge.i_ >= 0 && binary_params_) {
        int jumpsize = flen - edge.i_;
        const int fid = flen2jump2fid_[flen].find(jumpsize)->second;
        features->set_value(fid, 1.0);
      } else if (edge.i_ < 0 && binary_params_) {
        // handled by bigram features
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
    if (left_index == kNULL_i || right_index == kNULL_i) {
      if (left_index == kNULL_i && right_index == kNULL_i)
        features->set_value(fid_null_null_, 1.0);
      else if (left_index == kNULL_i)
        features->set_value(fid_null_lex_, 1.0);
      else
        features->set_value(fid_lex_null_, 1.0);

    } else {
      features->set_value(fid_lex_lex_, 1.0); // TODO should only use if NULL is enabled
      const int jumpsize = right_index - left_index;

      if (binary_params_) {
        const int fid = flen2jump2fid_[flen].find(jumpsize)->second;
        features->set_value(fid, 1.0);
      } else {
        features->set_value(fid_, fabs(jumpsize - 1));  // Blunsom and Cohn def
      }
    }
  } else {
    assert(!"something really unexpected is happening");
  }
}

NewJump::NewJump(const string& param) :
    FeatureFunction(1) {
  cerr << "    NewJump";
  vector<string> argv;
  int argc = SplitOnWhitespace(param, &argv);
  set<string> config;
  for (int i = 0; i < argc; ++i) config.insert(argv[i]);
  cerr << endl;
  use_binned_log_lengths_ = config.count("use_binned_log_lengths") > 0;
}

// do a log transform on the length (of a sentence, a jump, etc)
// this basically means that large distances that are close to each other
// are put into the same bin
int BinnedLogLength(int len) {
  int res = static_cast<int>(log(len+1) / log(1.3));
  if (res > 16) res = 16;
  return res;
}

void NewJump::FireFeature(const SentenceMetadata& smeta,
                          const int prev_src_index,
                          const int cur_src_index,
                          SparseVector<double>* features) const {
  const int src_len = smeta.GetSourceLength();
  const int raw_jump = cur_src_index - prev_src_index;
  char jtype = 0;
  int jump_magnitude = raw_jump;
  if (raw_jump > 0) { jtype = 'R'; } // Right
  else if (raw_jump == 0) { jtype = 'S'; } // Stay
  else { jtype = 'L'; jump_magnitude = raw_jump * -1; } // Left
  int effective_length = src_len;
  if (use_binned_log_lengths_) {
    jump_magnitude = BinnedLogLength(jump_magnitude);
    effective_length = BinnedLogLength(src_len);
  }

  if (true) {
    static map<int, map<int, int> > len2jump2fid;
    int& fid = len2jump2fid[src_len][raw_jump];
    if (!fid) {
      ostringstream os;
      os << fid_str_ << ":FLen" << effective_length << ":" << jtype << jump_magnitude;
      fid = FD::Convert(os.str());
    }
    features->set_value(fid, 1.0);
  }
}

void NewJump::TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                       const Hypergraph::Edge& edge,
                                       const vector<const void*>& ant_states,
                                       SparseVector<double>* features,
                                       SparseVector<double>* /* estimated_features */,
                                       void* state) const {
  unsigned char& dpstate = *((unsigned char*)state);
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
    os << "SB:";
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
    out_context = edge.rule_->f()[0];
    out_word_count = edge.rule_->EWords();
    assert(out_word_count == 1); // this is only defined for lex translation!
    // revisit this if you want to translate into null words
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
// state: POS of src word used, number of trg words generated
SourcePOSBigram::SourcePOSBigram(const std::string& param) :
    FeatureFunction(sizeof(WordID) + sizeof(int)) {
  cerr << "Reading source POS tags from " << param << endl;
  ReadFile rf(param);
  istream& in = *rf.stream();
  while(in) {
    string line;
    getline(in, line);
    if (line.empty()) continue;
    vector<WordID> v;
    TD::ConvertSentence(line, &v);
    pos_.push_back(v);
  }
  cerr << "  (" << pos_.size() << " lines)\n";
}

void SourcePOSBigram::FinalTraversalFeatures(const void* context,
                                      SparseVector<double>* features) const {
  WordID left = *static_cast<const WordID*>(context);
  int left_wc = *(static_cast<const int*>(context) + 1);
  if (left_wc == 1)
    FireFeature(-1, left, features);
  FireFeature(left, -1, features);
}

void SourcePOSBigram::FireFeature(WordID left,
                   WordID right,
                   SparseVector<double>* features) const {
  int& fid = fmap_[left][right];
  if (!fid) {
    ostringstream os;
    os << "SP:";
    if (left < 0) { os << "BOS"; } else { os << TD::Convert(left); }
    os << '_';
    if (right < 0) { os << "EOS"; } else { os << TD::Convert(right); }
    fid = FD::Convert(os.str());
    if (fid == 0) fid = -1;
  }
  if (fid < 0) return;
  features->set_value(fid, 1.0);
}

void SourcePOSBigram::TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                            SparseVector<double>* /* estimated_features */,
                                     void* context) const {
  WordID& out_context = *static_cast<WordID*>(context);
  int& out_word_count = *(static_cast<int*>(context) + 1);
  const int arity = edge.Arity();
  if (arity == 0) {
    assert(smeta.GetSentenceID() < pos_.size());
    const vector<WordID>& pos_sent = pos_[smeta.GetSentenceID()];
    if (edge.i_ >= 0) {  // non-NULL source
      assert(edge.i_ < pos_sent.size());
      out_context = pos_sent[edge.i_];
    } else { // NULL source
      // should assert that source is kNULL?
      static const WordID kNULL = TD::Convert("<eps>");
      out_context = kNULL;
    }
    out_word_count = edge.rule_->EWords();
    assert(out_word_count == 1); // this is only defined for lex translation!
    // revisit this if you want to translate into null words
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

// state: src word used, number of trg words generated
AlignerResults::AlignerResults(const std::string& param) :
    cur_sent_(-1),
    cur_grid_(NULL) {
  vector<string> argv;
  int argc = SplitOnWhitespace(param, &argv);
  if (argc != 2) {
    cerr << "Required format: AlignerResults [FeatureName] [file.pharaoh]\n";
    exit(1);
  }
  cerr << "  feature: " << argv[0] << "\talignments: " << argv[1] << endl;
  fid_ = FD::Convert(argv[0]);
  ReadFile rf(argv[1]);
  istream& in = *rf.stream(); int lc = 0;
  while(in) {
    string line;
    getline(in, line);
    if (!in) break;
    ++lc;
    is_aligned_.push_back(AlignmentPharaoh::ReadPharaohAlignmentGrid(line));
  }
  cerr << "  Loaded " << lc << " refs\n";
}

void AlignerResults::TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                           const Hypergraph::Edge& edge,
                                           const vector<const void*>& /* ant_states */,
                                           SparseVector<double>* features,
                                           SparseVector<double>* /* estimated_features */,
                                           void* /* state */) const {
  if (edge.i_ == -1 || edge.prev_i_ == -1)
    return;

  if (cur_sent_ != smeta.GetSentenceID()) {
    assert(smeta.HasReference());
    cur_sent_ = smeta.GetSentenceID();
    assert(cur_sent_ < is_aligned_.size());
    cur_grid_ = is_aligned_[cur_sent_].get();
  }

  //cerr << edge.rule_->AsString() << endl;

  int j = edge.i_;        // source side (f)
  int i = edge.prev_i_;   // target side (e)
  if (j < cur_grid_->height() && i < cur_grid_->width() && (*cur_grid_)(i, j)) {
//    if (edge.rule_->e_[0] == smeta.GetReference()[i][0].label) {
      features->set_value(fid_, 1.0);
//      cerr << edge.rule_->AsString() << "   (" << i << "," << j << ")\n";
//    }
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

InputIdentity::InputIdentity(const std::string& param) {}

void InputIdentity::FireFeature(WordID src,
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

void InputIdentity::TraversalFeaturesImpl(const SentenceMetadata& smeta,
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

