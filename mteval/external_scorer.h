#ifndef _EXTERNAL_SCORER_H_
#define _EXTERNAL_SCORER_H_

#include <vector>
#include <cstdio>

#include "scorer.h"

class ScoreServer {
 public:
  explicit ScoreServer(const std::string& cmd);
  virtual ~ScoreServer();

  double ComputeScore(const std::vector<float>& fields);
  void Evaluate(const std::vector<std::vector<WordID> >& refs, const std::vector<WordID>& hyp, std::vector<float>* fields);

 private:
  void RequestResponse(const std::string& request, std::string* response);
  FILE* pipe_;
};

class ExternalSentenceScorer : public SentenceScorer {
 public:
  virtual ScoreP ScoreCandidate(const Sentence& hyp) const = 0;
  virtual ScoreP ScoreCCandidate(const Sentence& hyp) const =0;
 protected:
  ScoreServer* eval_server;
};

class METEORServer : public ScoreServer {
 public:
  METEORServer() : ScoreServer("java -Xmx1024m -jar meteor-1.3.jar - - -mira -lower") {}
};

#endif
