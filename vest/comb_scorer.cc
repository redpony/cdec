#include "comb_scorer.h"

#include <cstdio>

using namespace std;

class BLEUTERCombinationScore : public Score {
  friend class BLEUTERCombinationScorer;
 public:
  ~BLEUTERCombinationScore();
  float ComputeScore() const {
    return (bleu->ComputeScore() - ter->ComputeScore()) / 2.0f;
  }
  void ScoreDetails(string* details) const {
    char buf[160];
    sprintf(buf, "Combi = %.2f, BLEU = %.2f, TER = %.2f", 
      ComputeScore()*100.0f, bleu->ComputeScore()*100.0f, ter->ComputeScore()*100.0f);
    *details = buf;
  }
  void PlusEquals(const Score& delta) {
    bleu->PlusEquals(*static_cast<const BLEUTERCombinationScore&>(delta).bleu);
    ter->PlusEquals(*static_cast<const BLEUTERCombinationScore&>(delta).ter);
  }
  Score* GetZero() const {
    BLEUTERCombinationScore* res = new BLEUTERCombinationScore;
    res->bleu = bleu->GetZero();
    res->ter = ter->GetZero();
    return res;
  }
  void Subtract(const Score& rhs, Score* res) const {
    bleu->Subtract(*static_cast<const BLEUTERCombinationScore&>(rhs).bleu,
                   static_cast<BLEUTERCombinationScore*>(res)->bleu);
    ter->Subtract(*static_cast<const BLEUTERCombinationScore&>(rhs).ter,
                   static_cast<BLEUTERCombinationScore*>(res)->ter);
  }
  void Encode(std::string* out) const {
    string bs, ts;
    bleu->Encode(&bs);
    ter->Encode(&ts);
    out->clear();
    (*out) += static_cast<char>(bs.size());
    (*out) += bs;
    (*out) += ts;
  }
  bool IsAdditiveIdentity() const {
    return bleu->IsAdditiveIdentity() && ter->IsAdditiveIdentity();
  }
 private:
  Score* bleu;
  Score* ter;
};

BLEUTERCombinationScore::~BLEUTERCombinationScore() {
  delete bleu;
  delete ter;
}

BLEUTERCombinationScorer::BLEUTERCombinationScorer(const vector<vector<WordID> >& refs) {
  bleu_ = SentenceScorer::CreateSentenceScorer(IBM_BLEU, refs);
  ter_ = SentenceScorer::CreateSentenceScorer(TER, refs);
}

BLEUTERCombinationScorer::~BLEUTERCombinationScorer() {
  delete bleu_;
  delete ter_;
}

Score* BLEUTERCombinationScorer::ScoreCandidate(const std::vector<WordID>& hyp) const {
  BLEUTERCombinationScore* res = new BLEUTERCombinationScore;
  res->bleu = bleu_->ScoreCandidate(hyp);
  res->ter = ter_->ScoreCandidate(hyp);
  return res;
}

Score* BLEUTERCombinationScorer::ScoreFromString(const std::string& in) {
  int bss = in[0];
  BLEUTERCombinationScore* r = new BLEUTERCombinationScore;
  r->bleu = SentenceScorer::CreateScoreFromString(IBM_BLEU, in.substr(1, bss));
  r->ter = SentenceScorer::CreateScoreFromString(TER, in.substr(1 + bss));
  return r;
}
