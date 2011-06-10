#include "external_scorer.h"

#include <cstdio> // popen
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sstream>
#include <iostream>
#include <cassert>

#include "stringlib.h"
#include "tdict.h"

using namespace std;

map<string, boost::shared_ptr<ScoreServer> > ScoreServerManager::servers_;

class METEORServer : public ScoreServer {
 public:
  METEORServer() : ScoreServer("java -Xmx1024m -jar /usr0/cdyer/meteor/meteor-1.3.jar - - -mira -lower -t tune -l en") {}
};

ScoreServer* ScoreServerManager::Instance(const string& score_type) {
  boost::shared_ptr<ScoreServer>& s = servers_[score_type];
  if (!s) {
    if (score_type == "meteor") {
      s.reset(new METEORServer);
    } else {
      cerr << "Don't know how to create score server for type '" << score_type << "'\n";
      abort();
    }
  }
  return s.get();
}

ScoreServer::ScoreServer(const string& cmd) {
  cerr << "Invoking " << cmd << " ..." << endl;
  if (pipe(p2c) < 0) { perror("pipe"); exit(1); }
  if (pipe(c2p) < 0) { perror("pipe"); exit(1); }
  pid_t cpid = fork();
  if (cpid < 0) { perror("fork"); exit(1); }
  if (cpid == 0) {  // child
    close(p2c[1]);
    close(c2p[0]);
    dup2(p2c[0], 0);
    close(p2c[0]);
    dup2(c2p[1], 1);
    close(c2p[1]);
    cerr << "Exec'ing from child " << cmd << endl;
    vector<string> vargs;
    SplitOnWhitespace(cmd, &vargs);
    const char** cargv = static_cast<const char**>(malloc(sizeof(const char*) * vargs.size()));
    for (unsigned i = 1; i < vargs.size(); ++i) cargv[i-1] = vargs[i].c_str();
    cargv[vargs.size() - 1] = NULL;
    execvp(vargs[0].c_str(), (char* const*)cargv);
  } else { // parent
    close(c2p[1]);
    close(p2c[0]);
  }
  string dummy;
  RequestResponse("SCORE ||| Reference initialization string . ||| Testing initialization string .", &dummy);
  assert(dummy.size() > 0);
  cerr << "Connection established.\n";
}

ScoreServer::~ScoreServer() {
  // TODO close stuff, join stuff
}

float ScoreServer::ComputeScore(const vector<float>& fields) {
  ostringstream os;
  os << "EVAL |||";
  for (unsigned i = 0; i < fields.size(); ++i)
    os << ' ' << fields[i];
  string sres;
  RequestResponse(os.str(), &sres);
  return strtod(sres.c_str(), NULL);
}

void ScoreServer::Evaluate(const vector<vector<WordID> >& refs, const vector<WordID>& hyp, vector<float>* fields) {
  ostringstream os;
  os << "SCORE";
  for (unsigned i = 0; i < refs.size(); ++i) {
    os << " |||";
    for (unsigned j = 0; j < refs[i].size(); ++j) {
      os << ' ' << TD::Convert(refs[i][j]);
    }
  }
  os << " |||";
  for (unsigned i = 0; i < hyp.size(); ++i) {
    os << ' ' << TD::Convert(hyp[i]);
  }
  string sres;
  RequestResponse(os.str(), &sres);
  istringstream is(sres);
  float val;
  fields->clear();
  while(is >> val)
    fields->push_back(val);
}

#define MAX_BUF 16000

void ScoreServer::RequestResponse(const string& request, string* response) {
//  cerr << "@SERVER: " << request << endl;
  string x = request + "\n";
  write(p2c[1], x.c_str(), x.size());
  char buf[MAX_BUF];
  size_t n = read(c2p[0], buf, MAX_BUF);
  while (n < MAX_BUF && buf[n-1] != '\n')
    n += read(c2p[0], &buf[n], MAX_BUF - n);

  buf[n-1] = 0;
  if (n < 2) {
    cerr << "Malformed response: " << buf << endl;
  }
  *response = Trim(buf, " \t\n");
//  cerr << "@RESPONSE: '" << *response << "'\n";
}

struct ExternalScore : public ScoreBase<ExternalScore> {
  ExternalScore() : score_server() {}
  explicit ExternalScore(ScoreServer* s) : score_server(s), fields() {}
  ExternalScore(ScoreServer* s, const vector<float>& f) : score_server(s), fields(f) {}
  float ComputePartialScore() const { return 0.0;}
  float ComputeScore() const {
    return score_server->ComputeScore(fields);
  }
  void ScoreDetails(string* details) const {
    ostringstream os;
    os << "EXT=" << ComputeScore() << " <";
    for (unsigned i = 0; i < fields.size(); ++i)
      os << (i ? " " : "") << fields[i];
    os << '>';
    *details = os.str();
  }
  void PlusPartialEquals(const Score&, int, int, int){
    assert(!"not implemented"); // no idea
  }
  void PlusEquals(const Score& delta, const float scale) {
    assert(!"not implemented"); // don't even know what this is
  }
  void PlusEquals(const Score& delta) {
    if (static_cast<const ExternalScore&>(delta).score_server) score_server = static_cast<const ExternalScore&>(delta).score_server;
    if (fields.size() != static_cast<const ExternalScore&>(delta).fields.size())
      fields.resize(max(fields.size(), static_cast<const ExternalScore&>(delta).fields.size()));
    for (unsigned i = 0; i < static_cast<const ExternalScore&>(delta).fields.size(); ++i)
      fields[i] += static_cast<const ExternalScore&>(delta).fields[i];
  }
  ScoreP GetZero() const {
    return ScoreP(new ExternalScore(score_server));
  }
  ScoreP GetOne() const {
    return ScoreP(new ExternalScore(score_server));
  }
  void Subtract(const Score& rhs, Score* res) const {
    static_cast<ExternalScore*>(res)->score_server = score_server;
    vector<float>& rf = static_cast<ExternalScore*>(res)->fields;
    rf.resize(max(fields.size(), static_cast<const ExternalScore&>(rhs).fields.size()));
    for (unsigned i = 0; i < rf.size(); ++i) {
      rf[i] = (i < fields.size() ? fields[i] : 0.0f) -
              (i < static_cast<const ExternalScore&>(rhs).fields.size() ? static_cast<const ExternalScore&>(rhs).fields[i] : 0.0f);
    }
  }
  void Encode(string* out) const {
    ostringstream os;
    for (unsigned i = 0; i < fields.size(); ++i)
      os << (i == 0 ? "" : " ") << fields[i];
    *out = os.str();
  }
  bool IsAdditiveIdentity() const {
    for (unsigned i = 0; i < fields.size(); ++i)
      if (fields[i]) return false;
    return true;
  }

  mutable ScoreServer* score_server;
  vector<float> fields;
};

ScoreP ExternalSentenceScorer::ScoreCandidate(const Sentence& hyp) const {
  ExternalScore* res = new ExternalScore(eval_server);
  eval_server->Evaluate(refs, hyp, &res->fields);
  return ScoreP(res);
}

ScoreP ExternalSentenceScorer::ScoreCCandidate(const Sentence& hyp) const {
  assert(!"not implemented");
}

ScoreP ExternalSentenceScorer::ScoreFromString(ScoreServer* s, const string& data) {
  istringstream is(data);
  vector<float> fields;
  float val;
  while(is >> val)
    fields.push_back(val);
  return ScoreP(new ExternalScore(s, fields));
}

