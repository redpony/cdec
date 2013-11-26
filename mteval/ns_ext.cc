#include "ns_ext.h"

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

struct NScoreServer {
  NScoreServer(const std::string& cmd);
  ~NScoreServer();

  float ComputeScore(const std::vector<float>& fields);
  void Evaluate(const std::vector<std::vector<WordID> >& refs, const std::vector<WordID>& hyp, std::vector<float>* fields);

 private:
  void RequestResponse(const std::string& request, std::string* response);
  int p2c[2];
  int c2p[2];
};

NScoreServer::NScoreServer(const string& cmd) {
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

NScoreServer::~NScoreServer() {
  // TODO close stuff, join stuff
}

float NScoreServer::ComputeScore(const vector<float>& fields) {
  ostringstream os;
  os << "EVAL |||";
  for (unsigned i = 0; i < fields.size(); ++i)
    os << ' ' << fields[i];
  string sres;
  RequestResponse(os.str(), &sres);
  return strtod(sres.c_str(), NULL);
}

void NScoreServer::Evaluate(const vector<vector<WordID> >& refs, const vector<WordID>& hyp, vector<float>* fields) {
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

void NScoreServer::RequestResponse(const string& request, string* response) {
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

void ExternalMetric::ComputeSufficientStatistics(const std::vector<WordID>& hyp,
                                           const std::vector<std::vector<WordID> >& refs,
                                           SufficientStats* out) const {
  eval_server->Evaluate(refs, hyp, &out->fields);
}

float ExternalMetric::ComputeScore(const SufficientStats& stats) const {
  return eval_server->ComputeScore(stats.fields);
}

ExternalMetric::ExternalMetric(const string& metric_name, const std::string& command) :
    EvaluationMetric(metric_name),
    eval_server(new NScoreServer(command)) {}

ExternalMetric::~ExternalMetric() {
  delete eval_server;
}
