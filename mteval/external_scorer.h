#ifndef _EXTERNAL_SCORER_H_
#define _EXTERNAL_SCORER_H_

#include <vector>
#include <string>
#include <map>
#include <boost/shared_ptr.hpp>

#include "scorer.h"

class ScoreServer {
  friend class ScoreServerManager;
 protected:
  explicit ScoreServer(const std::string& cmd);
  virtual ~ScoreServer();

 public:
  float ComputeScore(const std::vector<float>& fields);
  void Evaluate(const std::vector<std::vector<WordID> >& refs, const std::vector<WordID>& hyp, std::vector<float>* fields);

 private:
  void RequestResponse(const std::string& request, std::string* response);
  int p2c[2];
  int c2p[2];
};

class ScoreServerManager {
 public:
  static ScoreServer* Instance(const std::string& score_type);
 private:
  static std::map<std::string, boost::shared_ptr<ScoreServer> > servers_;
};

class ExternalSentenceScorer : public SentenceScorer {
 public:
  ExternalSentenceScorer(ScoreServer* server, const std::vector<std::vector<WordID> >& r) :
    SentenceScorer("External", r), eval_server(server) {}
  virtual ScoreP ScoreCandidate(const Sentence& hyp) const;
  virtual ScoreP ScoreCCandidate(const Sentence& hyp) const;
  static ScoreP ScoreFromString(ScoreServer* s, const std::string& data);

 protected:
  ScoreServer* eval_server;
};

#endif
