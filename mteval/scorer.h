#ifndef SCORER_H_
#define SCORER_H_
#include <vector>
#include <string>
#include <boost/shared_ptr.hpp>
//TODO: use intrusive shared_ptr in Score (because there are many of them on ErrorSurfaces)
#include "wordid.h"
#include "intrusive_refcount.hpp"

class Score;
class SentenceScorer;
typedef boost::intrusive_ptr<Score> ScoreP;
typedef boost::shared_ptr<SentenceScorer> ScorerP;

class ViterbiEnvelope;
class ErrorSurface;
class Hypergraph;  // needed for alignment

//TODO: BLEU N (N separate arg, not part of enum)?
enum ScoreType { IBM_BLEU, NIST_BLEU, Koehn_BLEU, TER, BLEU_minus_TER_over_2, SER, AER, IBM_BLEU_3, METEOR };
ScoreType ScoreTypeFromString(const std::string& st);
std::string StringFromScoreType(ScoreType st);

class Score : public boost::intrusive_refcount<Score> {
 public:
  virtual ~Score();
  virtual float ComputeScore() const = 0;
  virtual float ComputePartialScore() const =0;
  virtual void ScoreDetails(std::string* details) const = 0;
  std::string ScoreDetails() {
    std::string d;
    ScoreDetails(&d);
    return d;
  }
  virtual void TimesEquals(float scale); // only for bleu; for mira oracle
  /// same as rhs.TimesEquals(scale);PlusEquals(rhs) except doesn't modify rhs.
  virtual void PlusEquals(const Score& rhs, const float scale) = 0;
  virtual void PlusEquals(const Score& rhs) = 0;
  virtual void PlusPartialEquals(const Score& rhs, int oracle_e_cover, int oracle_f_cover, int src_len) = 0;
  virtual void Subtract(const Score& rhs, Score *res) const = 0;
  virtual ScoreP GetZero() const = 0;
  virtual ScoreP GetOne() const = 0;
  virtual bool IsAdditiveIdentity() const = 0; // returns true if adding this delta
                                      // to another score results in no score change
				      // under any circumstances
  virtual void Encode(std::string* out) const = 0;
  static ScoreP GetZero(ScoreType type);
  static ScoreP GetOne(ScoreType type);
  virtual ScoreP Clone() const = 0;
protected:
  Score() {  } // we define these explicitly because refcount is noncopyable
  Score(Score const&) {  }
};

//TODO: make sure default copy ctors for score types do what we want.
template <class Derived>
struct ScoreBase : public Score {
  ScoreP Clone() const  {
    return ScoreP(new Derived(dynamic_cast<Derived const&>(*this)));
  }
};

class SentenceScorer {
 public:
  typedef std::vector<WordID> Sentence;
  typedef std::vector<Sentence> Sentences;
  std::string desc;
  Sentences refs;
  explicit SentenceScorer(std::string desc="SentenceScorer_unknown", Sentences const& refs=Sentences()) : desc(desc),refs(refs) {  }
  std::string verbose_desc() const;
  virtual float ComputeRefLength(const Sentence& hyp) const; // default: avg of refs.length
  virtual ~SentenceScorer();
  virtual ScoreP GetOne() const;
  virtual ScoreP GetZero() const;
  virtual ScoreP ScoreCandidate(const Sentence& hyp) const = 0;
  virtual ScoreP ScoreCCandidate(const Sentence& hyp) const =0;
  virtual const std::string* GetSource() const;
  static ScoreP CreateScoreFromString(const ScoreType type, const std::string& in);
  static ScorerP CreateSentenceScorer(const ScoreType type,
    const std::vector<Sentence >& refs,
    const std::string& src = "");
};

//TODO: should be able to GetOne GetZero without supplying sentence (just type)
class DocScorer {
 friend class DocStreamScorer;
 public:
  virtual ~DocScorer();
  DocScorer() {  }
  virtual void Init(const ScoreType type,
            const std::vector<std::string>& ref_files,
            const std::string& src_file = "",
            bool verbose=false
    );
  DocScorer(const ScoreType type,
            const std::vector<std::string>& ref_files,
            const std::string& src_file = "",
            bool verbose=false
    )
  {
    Init(type,ref_files,src_file,verbose);
  }

  virtual int size() const { return scorers_.size(); }
  virtual ScorerP operator[](size_t i) const { return scorers_[i]; }
  virtual void update(const std::string& /*ref*/) {}
 private:
  std::vector<ScorerP> scorers_;
};

class DocStreamScorer : public DocScorer {
	public:
		~DocStreamScorer();
		void Init(const ScoreType type,
					const std::vector<std::string>& ref_files,
					const std::string& src_file = "",
					bool verbose=false
			);
		DocStreamScorer(const ScoreType type,
					const std::vector<std::string>& ref_files,
					const std::string& src_file = "",
					bool verbose=false
					)
		{
			Init(type,ref_files,src_file,verbose);
		}
		ScorerP operator[](size_t /*i*/) const { return scorer; }
		int size() const { return 1; }
		void update(const std::string& ref);
	private:
		ScoreType type;
		ScorerP scorer;
};

#endif
