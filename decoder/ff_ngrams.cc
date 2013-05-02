#include "ff_ngrams.h"

#include <cstring>
#include <iostream>

#include <boost/scoped_ptr.hpp>

#include "filelib.h"
#include "stringlib.h"
#include "hg.h"
#include "tdict.h"

using namespace std;

static const unsigned char HAS_FULL_CONTEXT = 1;
static const unsigned char HAS_EOS_ON_RIGHT = 2;
static const unsigned char MASK             = 7;

namespace {
template <unsigned MAX_ORDER = 5>
struct State {
  explicit State() {
    memset(state, 0, sizeof(state));
  }
  explicit State(int order) {
    memset(state, 0, (order - 1) * sizeof(WordID));
  }
  State<MAX_ORDER>(char order, const WordID* mem) {
    memcpy(state, mem, (order - 1) * sizeof(WordID));
  }
  State(const State<MAX_ORDER>& other) {
    memcpy(state, other.state, sizeof(state));
  }
  const State& operator=(const State<MAX_ORDER>& other) {
    memcpy(state, other.state, sizeof(state));
  }
  explicit State(const State<MAX_ORDER>& other, unsigned order, WordID extend) {
    char om1 = order - 1;
    assert(om1 > 0);
    for (char i = 1; i < om1; ++i) state[i - 1]= other.state[i];
    state[om1 - 1] = extend;
  }
  const WordID& operator[](size_t i) const { return state[i]; }
  WordID& operator[](size_t i) { return state[i]; }
  WordID state[MAX_ORDER];
};
}

namespace {
  string Escape(const string& x) {
    if (x.find('=') == string::npos && x.find(';') == string::npos) {
      return x;
    }
    string y = x;
    for (int i = 0; i < y.size(); ++i) {
      if (y[i] == '=') y[i]='_';
      if (y[i] == ';') y[i]='_';
    }
    return y;
  }
}

static bool ParseArgs(string const& in, bool* explicit_markers, unsigned* order, vector<string>& prefixes, string& target_separator, string* cluster_file) {
  vector<string> const& argv=SplitOnWhitespace(in);
  *explicit_markers = false;
  *order = 3;
  prefixes.push_back("NOT-USED");
  prefixes.push_back("U:"); // default unigram prefix
  prefixes.push_back("B:"); // default bigram prefix
  prefixes.push_back("T:"); // ...etc
  prefixes.push_back("4:"); // ...etc
  prefixes.push_back("5:"); // max allowed!
  target_separator = "_";
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
      case 'U':
	LMSPEC_NEXTARG;
	prefixes[1] = *i;
	break;
      case 'B':
	LMSPEC_NEXTARG;
	prefixes[2] = *i;
	break;
      case 'T':
	LMSPEC_NEXTARG;
	prefixes[3] = *i;
	break;
      case '4':
	LMSPEC_NEXTARG;
	prefixes[4] = *i;
	break;
      case '5':
	LMSPEC_NEXTARG;
	prefixes[5] = *i;
	break;
      case 'c':
        LMSPEC_NEXTARG;
        *cluster_file = *i;
        break;
      case 'S':
	LMSPEC_NEXTARG;
	target_separator = *i;
	break;
      case 'o':
        LMSPEC_NEXTARG; *order=atoi((*i).c_str());
        break;
#undef LMSPEC_NEXTARG
      default:
      fail:
        cerr<<"Unknown option on NgramFeatures "<<s<<" ; ";
        goto usage;
      }
    }
  }
  return true;
usage:
  cerr << "Wrong parameters for NgramFeatures.\n\n"

       << "NgramFeatures Usage: \n"			     
       << " feature_function=NgramFeatures filename.lm [-x] [-o <order>] \n"
       << " [-c <cluster-file>]\n"
       << " [-U <unigram-prefix>] [-B <bigram-prefix>][-T <trigram-prefix>]\n"
       << " [-4 <4-gram-prefix>] [-5 <5-gram-prefix>] [-S <separator>]\n\n" 
    
       << "Defaults: \n"
       << "  <order>          = 3\n" 
       << "  <unigram-prefix> = U:\n"
       << "  <bigram-prefix>  = B:\n"
       << "  <trigram-prefix> = T:\n"
       << "  <4-gram-prefix>  = 4:\n"
       << "  <5-gram-prefix>  = 5:\n"
       << "  <separator>      = _\n"
       << "  -x (i.e. explicit sos/eos markers) is turned off\n\n"

       << "Example configuration: \n"
       << "  feature_function=NgramFeatures -o 3 -T tri: -S |\n\n"

       << "Example feature instantiation: \n"
       << "  tri:a|b|c \n\n";

  return false;
}

class NgramDetectorImpl {

  // returns the number of unscored words at the left edge of a span
  inline int UnscoredSize(const void* state) const {
    return *(static_cast<const char*>(state) + unscored_size_offset_);
  }

  inline void SetUnscoredSize(int size, void* state) const {
    *(static_cast<char*>(state) + unscored_size_offset_) = size;
  }

  inline State<5> RemnantLMState(const void* cstate) const {
    return State<5>(order_, static_cast<const WordID*>(cstate));
  }

  inline const State<5> BeginSentenceState() const {
    State<5> state(order_);
    state.state[0] = kSOS_;
    return state;
  }

  inline void SetRemnantLMState(const State<5>& lmstate, void* state) const {
    // if we were clever, we could use the memory pointed to by state to do all
    // the work, avoiding this copy
    memcpy(state, lmstate.state, (order_-1) * sizeof(WordID));
  }

  WordID IthUnscoredWord(int i, const void* state) const {
    const WordID* const mem = reinterpret_cast<const WordID*>(static_cast<const char*>(state) + unscored_words_offset_);
    return mem[i];
  }

  void SetIthUnscoredWord(int i, const WordID index, void *state) const {
    WordID* mem = reinterpret_cast<WordID*>(static_cast<char*>(state) + unscored_words_offset_);
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

  WordID MapToClusterIfNecessary(WordID w) const {
    if (cluster_map.size() == 0) return w;
    if (w >= cluster_map.size()) return kCDEC_UNK;
    return cluster_map[w];
  }

  void FireFeatures(const State<5>& state, WordID cur, SparseVector<double>* feats) {
    FidTree* ft = &fidroot_;
    int n = 0;
    WordID buf[10];
    int ci = order_ - 1;
    WordID curword = cur;
    while(curword) {
      buf[n] = curword;
      int& fid = ft->fids[curword];
      ++n;
      if (!fid) {
        ostringstream os;
        os << prefixes_[n];
        for (int i = n-1; i >= 0; --i) {
          os << (i != n-1 ? target_separator_ : "");
          const string& tok = TD::Convert(buf[i]);
	  os << Escape(tok);
        }
        fid = FD::Convert(os.str());
      }
      feats->set_value(fid, 1);
      ft = &ft->levels[curword];
      --ci;
      if (ci < 0) break;
      curword = state[ci];
    }
  }

 public:
  void LookupWords(const TRule& rule, const vector<const void*>& ant_states, SparseVector<double>* feats, SparseVector<double>* est_feats, void* remnant) {
    double sum = 0.0;
    double est_sum = 0.0;
    int num_scored = 0;
    int num_estimated = 0;
    bool saw_eos = false;
    bool has_some_history = false;
    State<5> state;
    const vector<WordID>& e = rule.e();
    bool context_complete = false;
    for (int j = 0; j < e.size(); ++j) {
      if (e[j] < 1) {   // handle non-terminal substitution
        const void* astate = (ant_states[-e[j]]);
        int unscored_ant_len = UnscoredSize(astate);
        for (int k = 0; k < unscored_ant_len; ++k) {
          const WordID cur_word = IthUnscoredWord(k, astate);
          const bool is_oov = (cur_word == 0);
          SparseVector<double> p;
          if (cur_word == kSOS_) {
            state = BeginSentenceState();
            if (has_some_history) {  // this is immediately fully scored, and bad
              p.set_value(FD::Convert("Malformed"), 1.0);
              context_complete = true;
            } else {  // this might be a real <s>
              num_scored = max(0, order_ - 2);
            }
          } else {
            FireFeatures(state, cur_word, &p);
            const State<5> scopy = State<5>(state, order_, cur_word);
            state = scopy;
            if (saw_eos) { p.set_value(FD::Convert("Malformed"), 1.0); }
            saw_eos = (cur_word == kEOS_);
          }
          has_some_history = true;
          ++num_scored;
          if (!context_complete) {
            if (num_scored >= order_) context_complete = true;
          }
          if (context_complete) {
            (*feats) += p;
          } else {
            if (remnant)
              SetIthUnscoredWord(num_estimated, cur_word, remnant);
            ++num_estimated;
            (*est_feats) += p;
          }
        }
        saw_eos = GetFlag(astate, HAS_EOS_ON_RIGHT);
        if (HasFullContext(astate)) { // this is equivalent to the "star" in Chiang 2007
          state = RemnantLMState(astate);
          context_complete = true;
        }
      } else {   // handle terminal
        const WordID cur_word = MapToClusterIfNecessary(e[j]);
        SparseVector<double> p;
        if (cur_word == kSOS_) {
          state = BeginSentenceState();
          if (has_some_history) {  // this is immediately fully scored, and bad
            p.set_value(FD::Convert("Malformed"), -100);
            context_complete = true;
          } else {  // this might be a real <s>
            num_scored = max(0, order_ - 2);
          }
        } else {
          FireFeatures(state, cur_word, &p);
          const State<5> scopy = State<5>(state, order_, cur_word);
          state = scopy;
          if (saw_eos) { p.set_value(FD::Convert("Malformed"), 1.0); }
          saw_eos = (cur_word == kEOS_);
        }
        has_some_history = true;
        ++num_scored;
        if (!context_complete) {
          if (num_scored >= order_) context_complete = true;
        }
        if (context_complete) {
          (*feats) += p;
        } else {
          if (remnant)
            SetIthUnscoredWord(num_estimated, cur_word, remnant);
          ++num_estimated;
          (*est_feats) += p;
        }
      }
    }
    if (remnant) {
      SetFlag(saw_eos, HAS_EOS_ON_RIGHT, remnant);
      SetRemnantLMState(state, remnant);
      SetUnscoredSize(num_estimated, remnant);
      SetHasFullContext(context_complete || (num_scored >= order_), remnant);
    }
  }

  // this assumes no target words on final unary -> goal rule.  is that ok?
  // for <s> (n-1 left words) and (n-1 right words) </s>
  void FinalTraversal(const void* state, SparseVector<double>* feats) {
    if (add_sos_eos_) {  // rules do not produce <s> </s>, so do it here
      SetRemnantLMState(BeginSentenceState(), dummy_state_);
      SetHasFullContext(1, dummy_state_);
      SetUnscoredSize(0, dummy_state_);
      dummy_ants_[1] = state;
      LookupWords(*dummy_rule_, dummy_ants_, feats, NULL, NULL);
    } else {  // rules DO produce <s> ... </s>
#if 0
      double p = 0;
      if (!GetFlag(state, HAS_EOS_ON_RIGHT)) { p -= 100; }
      if (UnscoredSize(state) > 0) {  // are there unscored words
        if (kSOS_ != IthUnscoredWord(0, state)) {
          p -= 100 * UnscoredSize(state);
        }
      }
      return p;
#endif
    }
  }

  void ReadClusterFile(const string& clusters) {
    ReadFile rf(clusters);
    istream& in = *rf.stream();
    string line;
    int lc = 0;
    string cluster;
    string word;
    while(getline(in, line)) {
      ++lc;
      if (line.size() == 0) continue;
      if (line[0] == '#') continue;
      unsigned cend = 1;
      while((line[cend] != ' ' && line[cend] != '\t') && cend < line.size()) {
        ++cend;
      }
      if (cend == line.size()) {
        cerr << "Line " << lc << " in " << clusters << " malformed: " << line << endl;
        abort();
      }
      unsigned wbeg = cend + 1;
      while((line[wbeg] == ' ' || line[wbeg] == '\t') && wbeg < line.size()) {
        ++wbeg;
      }
      if (wbeg == line.size()) {
        cerr << "Line " << lc << " in " << clusters << " malformed: " << line << endl;
        abort();
      }
      unsigned wend = wbeg + 1;
      while((line[wend] != ' ' && line[wend] != '\t') && wend < line.size()) {
        ++wend;
      }
      const WordID clusterid = TD::Convert(line.substr(0, cend));
      const WordID wordid = TD::Convert(line.substr(wbeg, wend - wbeg));
      if (wordid >= cluster_map.size())
        cluster_map.resize(wordid + 10, kCDEC_UNK);
      cluster_map[wordid] = clusterid;
    }
    cluster_map[kSOS_] = kSOS_;
    cluster_map[kEOS_] = kEOS_;
  }

  vector<WordID> cluster_map;

 public:
  explicit NgramDetectorImpl(bool explicit_markers, unsigned order,
			     vector<string>& prefixes, string& target_separator, const string& clusters) :
      kCDEC_UNK(TD::Convert("<unk>")) ,
      add_sos_eos_(!explicit_markers) {
    order_ = order;
    state_size_ = (order_ - 1) * sizeof(WordID) + 2 + (order_ - 1) * sizeof(WordID);
    unscored_size_offset_ = (order_ - 1) * sizeof(WordID);
    is_complete_offset_ = unscored_size_offset_ + 1;
    unscored_words_offset_ = is_complete_offset_ + 1;
    prefixes_ = prefixes;
    target_separator_ = target_separator;

    // special handling of beginning / ending sentence markers
    dummy_state_ = new char[state_size_];
    memset(dummy_state_, 0, state_size_);
    dummy_ants_.push_back(dummy_state_);
    dummy_ants_.push_back(NULL);
    dummy_rule_.reset(new TRule("[DUMMY] ||| [BOS] [DUMMY] ||| [1] [2] </s> ||| X=0"));
    kSOS_ = TD::Convert("<s>");
    kEOS_ = TD::Convert("</s>");

    if (clusters.size())
      ReadClusterFile(clusters);
  }

  ~NgramDetectorImpl() {
    delete[] dummy_state_;
  }

  int ReserveStateSize() const { return state_size_; }

 private:
  const WordID kCDEC_UNK;
  WordID kSOS_;  // <s> - requires special handling.
  WordID kEOS_;  // </s>
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
  TRulePtr dummy_rule_;
  vector<string> prefixes_;
  string target_separator_;
  struct FidTree {
    map<WordID, int> fids;
    map<WordID, FidTree> levels;
  };
  mutable FidTree fidroot_;
};

NgramDetector::NgramDetector(const string& param) {
  string filename, mapfile, featname, target_separator;
  vector<string> prefixes;
  bool explicit_markers = false;
  unsigned order = 3;
  string clusters;
  ParseArgs(param, &explicit_markers, &order, prefixes, target_separator, &clusters);
  pimpl_ = new NgramDetectorImpl(explicit_markers, order, prefixes, 
				 target_separator, clusters);
  SetStateSize(pimpl_->ReserveStateSize());
}

NgramDetector::~NgramDetector() {
  delete pimpl_;
}

void NgramDetector::TraversalFeaturesImpl(const SentenceMetadata& /* smeta */,
                                          const Hypergraph::Edge& edge,
                                          const vector<const void*>& ant_states,
                                          SparseVector<double>* features,
                                          SparseVector<double>* estimated_features,
                                          void* state) const {
  pimpl_->LookupWords(*edge.rule_, ant_states, features, estimated_features, state);
}

void NgramDetector::FinalTraversalFeatures(const void* ant_state,
                                           SparseVector<double>* features) const {
  pimpl_->FinalTraversal(ant_state, features);
}

