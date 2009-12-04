#include "ff_lm.h"

#include <sstream>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>

#include "tdict.h"
#include "Vocab.h"
#include "Ngram.h"
#include "hg.h"
#include "stringlib.h"

using namespace std;

struct LMClient {
  struct Cache {
    map<WordID, Cache> tree;
    float prob;
    Cache() : prob() {}
  };

  LMClient(const char* host) : port(6666) {
    s = strchr(host, ':');
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
    Cache* cur = &cache;
    int i = 0;
    while (context[i] > 0) {
      cur = &cur->tree[context[i++]];
    }
    cur = &cur->tree[word];
    if (cur->prob) { return cur->prob; }

    i = 0;
    ostringstream os;
    os << "prob " << TD::Convert(word);
    while (context[i] > 0) {
      os << ' ' << TD::Convert(context[i++]);
    }
    os << endl;
    string out = os.str();
    write(sock, out.c_str(), out.size());
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

  void clear() {
    cache.tree.clear();
  }

 private:
  Cache cache;
  int sock, port;
  char *s;
  struct hostent *hp;
  struct sockaddr_in server;
  char res[8];
};

class LanguageModelImpl {
 public:
  LanguageModelImpl(int order, const string& f) :
      ngram_(*TD::dict_), buffer_(), order_(order), state_size_(OrderToStateSize(order) - 1),
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

  ~LanguageModelImpl() {
    delete client_;
  }

  inline int StateSize(const void* state) const {
    return *(static_cast<const char*>(state) + state_size_);
  }

  inline void SetStateSize(int size, void* state) const {
    *(static_cast<char*>(state) + state_size_) = size;
  }

  inline double LookupProbForBufferContents(int i) {
    double p = client_ ?
          client_->wordProb(buffer_[i], &buffer_[i+1])
        : ngram_.wordProb(buffer_[i], (VocabIndex*)&buffer_[i+1]);
    if (p < floor_) p = floor_;
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

 private:
  Ngram ngram_;
  vector<WordID> buffer_;
  const int order_;
  const int state_size_;
  const double floor_;
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

