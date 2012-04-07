#include "aer_scorer.h"

#include <cmath>
#include <cassert>
#include <sstream>

#include "tdict.h"
#include "alignment_io.h"

using namespace std;

class AERScore : public ScoreBase<AERScore> {
  friend class AERScorer;
 public:
  AERScore() : num_matches(), num_predicted(), num_in_ref() {}
  AERScore(int m, int p, int r) :
    num_matches(m), num_predicted(p), num_in_ref(r) {}
  virtual void PlusPartialEquals(const Score& rhs, int oracle_e_cover, int oracle_f_cover, int src_len){}
  virtual void PlusEquals(const Score& delta, const float scale) {
    const AERScore& other = static_cast<const AERScore&>(delta);
    num_matches   += scale*other.num_matches;
    num_predicted += scale*other.num_predicted;
    num_in_ref    += scale*other.num_in_ref;
  }
 virtual void PlusEquals(const Score& delta) {
    const AERScore& other = static_cast<const AERScore&>(delta);
    num_matches   += other.num_matches;
    num_predicted += other.num_predicted;
    num_in_ref    += other.num_in_ref;
  }


  virtual ScoreP GetZero() const {
    return ScoreP(new AERScore);
  }
  virtual ScoreP GetOne() const {
    return ScoreP(new AERScore);
  }
  virtual void Subtract(const Score& rhs, Score* out) const {
    AERScore* res = static_cast<AERScore*>(out);
    const AERScore& other = static_cast<const AERScore&>(rhs);
    res->num_matches   = num_matches   - other.num_matches;
    res->num_predicted = num_predicted - other.num_predicted;
    res->num_in_ref    = num_in_ref    - other.num_in_ref;
  }
  float Precision() const {
    return static_cast<float>(num_matches) / num_predicted;
  }
  float Recall() const {
    return static_cast<float>(num_matches) / num_in_ref;
  }
  float ComputePartialScore() const { return 0.0;}
  virtual float ComputeScore() const {
    const float prec = Precision();
    const float rec = Recall();
    const float f = (2.0 * prec * rec) / (rec + prec);
    if (isnan(f)) return 1.0f;
    return 1.0f - f;
  }
  virtual bool IsAdditiveIdentity() const {
    return (num_matches == 0) && (num_predicted == 0) && (num_in_ref == 0);
  }
  virtual void ScoreDetails(std::string* out) const {
    ostringstream os;
    os << "AER=" << (ComputeScore() * 100.0)
       << " F=" << (100 - ComputeScore() * 100.0)
       << " P=" << (Precision() * 100.0) << " R=" << (Recall() * 100.0)
       << " [" << num_matches << " " << num_predicted << " " << num_in_ref << "]";
    *out = os.str();
  }
  virtual void Encode(std::string*out) const {
    out->resize(sizeof(int) * 3);
    *(int *)&(*out)[sizeof(int) * 0] = num_matches;
    *(int *)&(*out)[sizeof(int) * 1] = num_predicted;
    *(int *)&(*out)[sizeof(int) * 2] = num_in_ref;
  }
 private:
  int num_matches;
  int num_predicted;
  int num_in_ref;
};

AERScorer::AERScorer(const vector<vector<WordID> >& refs, const string& src) : src_(src) {
  if (refs.size() != 1) {
    cerr << "AERScorer can only take a single reference!\n";
    abort();
  }
  ref_ = AlignmentIO::ReadPharaohAlignmentGrid(TD::GetString(refs.front()));
}

static inline bool Safe(const Array2D<bool>& a, int i, int j) {
  if (i >= 0 && j >= 0 && i < a.width() && j < a.height())
    return a(i,j);
  else
    return false;
}

ScoreP AERScorer::ScoreCCandidate(const vector<WordID>& shyp) const {
  return ScoreP();
}

ScoreP AERScorer::ScoreCandidate(const vector<WordID>& shyp) const {
  boost::shared_ptr<Array2D<bool> > hyp =
    AlignmentIO::ReadPharaohAlignmentGrid(TD::GetString(shyp));

  int m = 0;
  int r = 0;
  int p = 0;
  int i_len = ref_->width();
  int j_len = ref_->height();
  for (int i = 0; i < i_len; ++i) {
    for (int j = 0; j < j_len; ++j) {
      if ((*ref_)(i,j)) {
        ++r;
        if (Safe(*hyp, i, j)) ++m;
      }
    }
  }
  for (int i = 0; i < hyp->width(); ++i)
    for (int j = 0; j < hyp->height(); ++j)
      if ((*hyp)(i,j)) ++p;

  return ScoreP(new AERScore(m,p,r));
}

ScoreP AERScorer::ScoreFromString(const string& in) {
  AERScore* res = new AERScore;
  res->num_matches   = *(const int *)&in[sizeof(int) * 0];
  res->num_predicted = *(const int *)&in[sizeof(int) * 1];
  res->num_in_ref    = *(const int *)&in[sizeof(int) * 2];
  return ScoreP(res);
}

const std::string* AERScorer::GetSource() const { return &src_; }

