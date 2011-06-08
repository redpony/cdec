#include "external_scorer.h"

#include <cstdio> // popen
#include <cstdlib>
#include <sstream>
#include <iostream>
#include <cassert>

#include "tdict.h"

using namespace std;

ScoreServer::ScoreServer(const string& cmd) : pipe_() {
  cerr << "Invoking " << cmd << " ..." << endl;
  pipe_ = popen(cmd.c_str(), "r+");
  assert(pipe_);
  string dummy;
  RequestResponse("EVAL ||| Reference initialization string . ||| Testing initialization string .\n", &dummy);
  assert(dummy.size() > 0);
  cerr << "Connection established.\n";
}

ScoreServer::~ScoreServer() {
  pclose(pipe_);
}

double ScoreServer::ComputeScore(const vector<float>& fields) {
  ostringstream os;
  os << "EVAL";
  for (unsigned i = 0; i < fields.size(); ++i)
    os << ' ' << fields[i];
  os << endl;
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
  os << endl;
  string sres;
  RequestResponse(os.str(), &sres);
  istringstream is(sres);
  double val;
  fields->clear();
  while(is >> val) {
    fields->push_back(val);
  }
}

#define MAX_BUF 16000

void ScoreServer::RequestResponse(const string& request, string* response) {
  fprintf(pipe_, "%s", request.c_str());
  fflush(pipe_);
  char buf[MAX_BUF];
  size_t cr = fread(buf, 1, MAX_BUF, pipe_);
  if (cr == 0) {
    cerr << "Read error. Request: " << request << endl;
    abort();
  }
  while (buf[cr-1] != '\n') {
    size_t n = fread(&buf[cr], 1, MAX_BUF-cr, pipe_);
    assert(n > 0);
    cr += n;
    assert(cr < MAX_BUF);
  }
  buf[cr - 1] = 0;
  *response = buf;
}

struct ExternalScore : public ScoreBase<ExternalScore> {
  ExternalScore() : score_server() {}
  explicit ExternalScore(const ScoreServer* s) : score_server(s), fields() {}
  ExternalScore(const ScoreServer* s, const vector<float>& f) : score_server(s), fields(f) {}
  float ComputePartialScore() const { return 0.0;}
  float ComputeScore() const {
    // TODO make EVAL call
    assert(!"not implemented");
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
  }
  bool IsAdditiveIdentity() const {
    for (int i = 0; i < fields.size(); ++i)
      if (fields[i]) return false;
    return true;
  }

  const ScoreServer* score_server;
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

