#include "ff_lm.h"

#include <sstream>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>

#include <boost/shared_ptr.hpp>

#include "tdict.h"
#include "Vocab.h"
#include "Ngram.h"
#include "hg.h"
#include "stringlib.h"

#ifdef HAVE_RANDLM
#include "RandLM.h"
#endif

using namespace std;

namespace NgramCache {
  struct Cache {
    map<WordID, Cache> tree;
    float prob;
    Cache() : prob() {}
  };
  static Cache cache_;
  void Clear() { cache_.tree.clear(); }
}

struct LMClient {

  LMClient(const char* host) : port(6666) {
    strcpy(request_buffer, "prob ");
    s = const_cast<char*>(strchr(host, ':'));  // TODO fix const_cast
    if (s != NULL) {
      *s = '\0';
      ++s;
      port = atoi(s);
    }
    sock = socket(AF_INET, SOCK_STREAM, 0);
    hp = gethostbyname(host);
    if (hp == NULL) {
      cerr << "unknown host " << host << endl;
      abort();
    }
    bzero((char *)&server, sizeof(server));
    bcopy(hp->h_addr, (char *)&server.sin_addr, hp->h_length);
    server.sin_family = hp->h_addrtype;
    server.sin_port = htons(port);

    int errors = 0;
    while (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
      cerr << "Error: connect()\n";
      sleep(1);
      errors++;
      if (errors > 3) exit(1);
    }
    cerr << "Connected to LM on " << host << " on port " << port << endl;
  }

  float wordProb(int word, int* context) {
    NgramCache::Cache* cur = &NgramCache::cache_;
    int i = 0;
    while (context[i] > 0) {
      cur = &cur->tree[context[i++]];
    }
    cur = &cur->tree[word];
    if (cur->prob) { return cur->prob; }

    i = 0;
    int pos = TD::AppendString(word, 5, 16000, request_buffer);
    while (context[i] > 0) {
      assert(pos < 15995);
      request_buffer[pos] = ' ';
      ++pos;
      pos = TD::AppendString(context[i], pos, 16000, request_buffer);
      ++i;
    }
    assert(pos < 15999);
    request_buffer[pos] = '\n';
    ++pos;
    request_buffer[pos] = 0;
    write(sock, request_buffer, pos);
    int r = read(sock, res, 6);
    int errors = 0;
    int cnt = 0;
    while (1) {
      if (r < 0) {
        errors++; sleep(1);
        cerr << "Error: read()\n";
        if (errors > 5) exit(1);
      } else if (r==0 || res[cnt] == '\n') { break; }
      else {
        cnt += r;
        if (cnt==6) break;
        read(sock, &res[cnt], 6-cnt);
      }
    }
    cur->prob = *reinterpret_cast<float*>(res);
    return cur->prob;
  }

 private:
  int sock, port;
  char *s;
  struct hostent *hp;
  struct sockaddr_in server;
  char res[8];
  char request_buffer[16000];
};

class LanguageModelImpl {
 public:
  explicit LanguageModelImpl(int order) :
      ngram_(*TD::dict_, order), buffer_(), order_(order), state_size_(OrderToStateSize(order) - 1),
      floor_(-100.0),
      client_(),
      kSTART(TD::Convert("<s>")),
      kSTOP(TD::Convert("</s>")),
      kUNKNOWN(TD::Convert("<unk>")),
      kNONE(-1),
      kSTAR(TD::Convert("<{STAR}>")) {}

  LanguageModelImpl(int order, const string& f) :
      ngram_(*TD::dict_, order), buffer_(), order_(order), state_size_(OrderToStateSize(order) - 1),
      floor_(-100.0),
      client_(NULL),
      kSTART(TD::Convert("<s>")),
      kSTOP(TD::Convert("</s>")),
      kUNKNOWN(TD::Convert("<unk>")),
      kNONE(-1),
      kSTAR(TD::Convert("<{STAR}>")) {
    if (f.find("lm://") == 0) {
      client_ = new LMClient(f.substr(5).c_str());
    } else {
      File file(f.c_str(), "r", 0);
      assert(file);
      cerr << "Reading " << order_ << "-gram LM from " << f << endl;
      ngram_.read(file, false);
    }
  }

  virtual ~LanguageModelImpl() {
    delete client_;
  }

  inline int StateSize(const void* state) const {
    return *(static_cast<const char*>(state) + state_size_);
  }

  inline void SetStateSize(int size, void* state) const {
    *(static_cast<char*>(state) + state_size_) = size;
  }

  virtual double WordProb(int word, int* context) {
    return client_ ?
          client_->wordProb(word, context)
        : ngram_.wordProb(word, (VocabIndex*)context);
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

  double EstimateProb(const void* state) {
    int len = StateSize(state);
    // cerr << "residual len: " << len << endl;
    buffer_.resize(len + 1);
    buffer_[len] = kNONE;
    const int* astate = reinterpret_cast<const int*>(state);
    int i = len - 1;
    for (int j = 0; j < len; ++j,--i)
      buffer_[i] = astate[j];
    return ProbNoRemnant(len - 1, len);
  }

  double FinalTraversalCost(const void* state) {
    int slen = StateSize(state);
    int len = slen + 2;
    // cerr << "residual len: " << len << endl;
    buffer_.resize(len + 1);
    buffer_[len] = kNONE;
    buffer_[len-1] = kSTART;
    const int* astate = reinterpret_cast<const int*>(state);
    int i = len - 2;
    for (int j = 0; j < slen; ++j,--i)
      buffer_[i] = astate[j];
    buffer_[i] = kSTOP;
    assert(i == 0);
    return ProbNoRemnant(len - 1, len);
  }

  double LookupWords(const TRule& rule, const vector<const void*>& ant_states, void* vstate) {
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

  static int OrderToStateSize(int order) {
    return ((order-1) * 2 + 1) * sizeof(WordID) + 1;
  }

 protected:
  Ngram ngram_;
  vector<WordID> buffer_;
  const int order_;
  const int state_size_;
  const double floor_;
 private:
  LMClient* client_;

 public:
  const WordID kSTART;
  const WordID kSTOP;
  const WordID kUNKNOWN;
  const WordID kNONE;
  const WordID kSTAR;
};

LanguageModel::LanguageModel(const string& param) :
    fid_(FD::Convert("LanguageModel")) {
  vector<string> argv;
  int argc = SplitOnWhitespace(param, &argv);
  int order = 3;
  // TODO add support for -n FeatureName
  string filename;
  if (argc < 1) { cerr << "LanguageModel requires a filename, minimally!\n"; abort(); }
  else if (argc == 1) { filename = argv[0]; }
  else if (argc == 2 || argc > 3) { cerr << "Don't understand 'LanguageModel " << param << "'\n"; }
  else if (argc == 3) {
    if (argv[0] == "-o") {
      order = atoi(argv[1].c_str());
      filename = argv[2];
    } else if (argv[1] == "-o") {
      order = atoi(argv[2].c_str());
      filename = argv[0];
    }
  }
  SetStateSize(LanguageModelImpl::OrderToStateSize(order));
  pimpl_ = new LanguageModelImpl(order, filename);
}

LanguageModel::~LanguageModel() {
  delete pimpl_;
}

string LanguageModel::DebugStateToString(const void* state) const{
  return pimpl_->DebugStateToString(state);
}

void LanguageModel::TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                          const Hypergraph::Edge& edge,
                                          const vector<const void*>& ant_states,
                                          SparseVector<double>* features,
                                          SparseVector<double>* estimated_features,
                                          void* state) const {
  (void) smeta;
  features->set_value(fid_, pimpl_->LookupWords(*edge.rule_, ant_states, state));
  estimated_features->set_value(fid_, pimpl_->EstimateProb(state));
}

void LanguageModel::FinalTraversalFeatures(const void* ant_state,
                                           SparseVector<double>* features) const {
  features->set_value(fid_, pimpl_->FinalTraversalCost(ant_state));
}

#ifdef HAVE_RANDLM
struct RandLMImpl : public LanguageModelImpl {
  RandLMImpl(int order, randlm::RandLM* rlm) :
      LanguageModelImpl(order),
      rlm_(rlm),
      oov_(rlm->getWordID(rlm->getOOV())),
      rb_(1000, oov_) {
    map<int, randlm::WordID> map_cdec2randlm;
    int max_wordid = 0;
    for(map<randlm::Word, randlm::WordID>::const_iterator it = rlm->vocabStart();
        it != rlm->vocabEnd(); ++it) {
      const int cur = TD::Convert(it->first);
      map_cdec2randlm[TD::Convert(it->first)] = it->second;
      if (cur > max_wordid) max_wordid = cur;
    }
    cdec2randlm_.resize(max_wordid + 1, oov_);
    for (map<int, randlm::WordID>::iterator it = map_cdec2randlm.begin();
         it != map_cdec2randlm.end(); ++it)
      cdec2randlm_[it->first] = it->second;
    map_cdec2randlm.clear();
  }

  inline randlm::WordID Convert2RandLM(int w) {
    return (w < cdec2randlm_.size() ? cdec2randlm_[w] : oov_);
  }

  virtual double WordProb(int word, int* context) {
    int i = order_;
    int c = 1;
    rb_[i] = Convert2RandLM(word);
    while (i > 1 && *context > 0) {
      --i;
      rb_[i] = Convert2RandLM(*context);
      ++context;
      ++c;
    }
    const void* finalState = 0;
    int found;
    //cerr << "I = " << i << endl;
    return rlm_->getProb(&rb_[i], c, &found, &finalState);
  }
 private:
  boost::shared_ptr<randlm::RandLM> rlm_;
  randlm::WordID oov_;
  vector<randlm::WordID> cdec2randlm_;
  vector<randlm::WordID> rb_;
};

LanguageModelRandLM::LanguageModelRandLM(const string& param) :
    fid_(FD::Convert("RandLM")) {
  vector<string> argv;
  int argc = SplitOnWhitespace(param, &argv);
  int order = 3;
  // TODO add support for -n FeatureName
  string filename;
  if (argc < 1) { cerr << "RandLM requires a filename, minimally!\n"; abort(); }
  else if (argc == 1) { filename = argv[0]; }
  else if (argc == 2 || argc > 3) { cerr << "Don't understand 'RandLM " << param << "'\n"; }
  else if (argc == 3) {
    if (argv[0] == "-o") {
      order = atoi(argv[1].c_str());
      filename = argv[2];
    } else if (argv[1] == "-o") {
      order = atoi(argv[2].c_str());
      filename = argv[0];
    }
  }
  SetStateSize(LanguageModelImpl::OrderToStateSize(order));
  int cache_MB = 200; // increase cache size
  randlm::RandLM* rlm = randlm::RandLM::initRandLM(filename, order, cache_MB);
  assert(rlm != NULL);
  pimpl_ = new RandLMImpl(order, rlm);
}

LanguageModelRandLM::~LanguageModelRandLM() {
  delete pimpl_;
}

void LanguageModelRandLM::TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                          const Hypergraph::Edge& edge,
                                          const vector<const void*>& ant_states,
                                          SparseVector<double>* features,
                                          SparseVector<double>* estimated_features,
                                          void* state) const {
  (void) smeta;
  features->set_value(fid_, pimpl_->LookupWords(*edge.rule_, ant_states, state));
  estimated_features->set_value(fid_, pimpl_->EstimateProb(state));
}

void LanguageModelRandLM::FinalTraversalFeatures(const void* ant_state,
                                           SparseVector<double>* features) const {
  features->set_value(fid_, pimpl_->FinalTraversalCost(ant_state));
}

#endif

