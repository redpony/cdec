#include "scorer.h"

#include <boost/lexical_cast.hpp>
#include <map>
#include <sstream>
#include <iostream>
#include <fstream>
#include <cstdio>
#include <valarray>
#include <algorithm>

#include <boost/shared_ptr.hpp>

#include "filelib.h"
#include "ter.h"
#include "aer_scorer.h"
#include "comb_scorer.h"
#include "tdict.h"
#include "stringlib.h"
#include "external_scorer.h"

using boost::shared_ptr;
using namespace std;

void Score::TimesEquals(float /*scale*/) {
  cerr<<"UNIMPLEMENTED except for BLEU (for MIRA): Score::TimesEquals"<<endl;abort();
}

ScoreType ScoreTypeFromString(const string& st) {
  const string sl = LowercaseString(st);
  if (sl == "ser")
    return SER;
  if (sl == "ter")
    return TER;
  if (sl == "aer")
    return AER;
  if (sl == "bleu" || sl == "ibm_bleu")
    return IBM_BLEU;
  if (sl == "ibm_bleu_3")
    return IBM_BLEU_3;
  if (sl == "nist_bleu")
    return NIST_BLEU;
  if (sl == "koehn_bleu")
    return Koehn_BLEU;
  if (sl == "combi")
    return BLEU_minus_TER_over_2;
  if (sl == "meteor")
    return METEOR;
  cerr << "Don't understand score type '" << st << "', defaulting to ibm_bleu.\n";
  return IBM_BLEU;
}

static char const* score_names[]={
  "IBM_BLEU", "NIST_BLEU", "Koehn_BLEU", "TER", "BLEU_minus_TER_over_2", "SER", "AER", "IBM_BLEU_3", "METEOR"
};

std::string StringFromScoreType(ScoreType st) {
  assert(st>=0 && st<sizeof(score_names)/sizeof(score_names[0]));
  return score_names[(int)st];
}


Score::~Score() {}
SentenceScorer::~SentenceScorer() {}

struct length_accum {
  template <class S>
  float operator()(float sum,S const& ref) const {
    return sum+ref.size();
  }
};

template <class S>
float avg_reflength(vector<S> refs) {
  unsigned n=refs.size();
  return n?accumulate(refs.begin(),refs.end(),0.,length_accum())/n:0.;
}


float SentenceScorer::ComputeRefLength(const Sentence &hyp) const {
  return hyp.size(); // reasonable default? :)
}

const std::string* SentenceScorer::GetSource() const { return NULL; }

class SERScore : public ScoreBase<SERScore> {
  friend class SERScorer;
 public:
  SERScore() : correct(0), total(0) {}
  float ComputePartialScore() const { return 0.0;}
  float ComputeScore() const {
    return static_cast<float>(correct) / static_cast<float>(total);
  }
  void ScoreDetails(string* details) const {
    ostringstream os;
    os << "SER= " << ComputeScore() << " (" << correct << '/' << total << ')';
    *details = os.str();
  }
  void PlusPartialEquals(const Score& /* delta */, int /* oracle_e_cover */, int /* oracle_f_cover */, int /* src_len */){}

  void PlusEquals(const Score& delta, const float scale) {
    correct += scale*static_cast<const SERScore&>(delta).correct;
    total += scale*static_cast<const SERScore&>(delta).total;
  }
  void PlusEquals(const Score& delta) {
    correct += static_cast<const SERScore&>(delta).correct;
    total += static_cast<const SERScore&>(delta).total;
    }
  ScoreP GetZero() const { return ScoreP(new SERScore); }
  ScoreP GetOne() const { return ScoreP(new SERScore); }
  void Subtract(const Score& rhs, Score* res) const {
    SERScore* r = static_cast<SERScore*>(res);
    r->correct = correct - static_cast<const SERScore&>(rhs).correct;
    r->total = total - static_cast<const SERScore&>(rhs).total;
  }
  void Encode(string* out) const {
    assert(!"not implemented");
  }
  bool IsAdditiveIdentity() const {
    return (total == 0 && correct == 0);  // correct is always 0 <= n <= total
  }
 private:
  int correct, total;
};

std::string SentenceScorer::verbose_desc() const {
  return desc+",ref0={ "+TD::GetString(refs[0])+" }";
}

class SERScorer : public SentenceScorer {
 public:
  SERScorer(const vector<vector<WordID> >& references) : SentenceScorer("SERScorer",references),refs_(references) {}
  ScoreP ScoreCCandidate(const vector<WordID>& /* hyp */) const {
    return ScoreP();
  }
  ScoreP ScoreCandidate(const vector<WordID>& hyp) const {
    SERScore* res = new SERScore;
    res->total = 1;
    for (int i = 0; i < refs_.size(); ++i)
      if (refs_[i] == hyp) res->correct = 1;
    return ScoreP(res);
  }
  static ScoreP ScoreFromString(const string& data) {
    assert(!"Not implemented");
  }
 private:
  vector<vector<WordID> > refs_;
};

class BLEUScore : public ScoreBase<BLEUScore> {
  friend class BLEUScorerBase;
 public:
  BLEUScore(int n) : correct_ngram_hit_counts(float(0),n), hyp_ngram_counts(float(0),n) {
    ref_len = 0;
    hyp_len = 0; }
  BLEUScore(int n, int k) :  correct_ngram_hit_counts(float(k),n), hyp_ngram_counts(float(k),n) {
    ref_len = k;
    hyp_len = k; }
  float ComputeScore() const;
  float ComputePartialScore() const;
  void ScoreDetails(string* details) const;
  void TimesEquals(float scale);
  void PlusEquals(const Score& delta);
  void PlusEquals(const Score& delta, const float scale);
  void PlusPartialEquals(const Score& delta, int oracle_e_cover, int oracle_f_cover, int src_len);
  ScoreP GetZero() const;
  ScoreP GetOne() const;
  void Subtract(const Score& rhs, Score* res) const;
  void Encode(string* out) const;
  bool IsAdditiveIdentity() const {
    if (fabs(ref_len) > 0.1f || hyp_len != 0) return false;
    for (int i = 0; i < correct_ngram_hit_counts.size(); ++i)
      if (hyp_ngram_counts[i] != 0 ||
        correct_ngram_hit_counts[i] != 0) return false;
    return true;
  }
 private:
  int N() const {
    return hyp_ngram_counts.size();
  }
  float ComputeScore(vector<float>* precs, float* bp) const;
  float ComputePartialScore(vector<float>* prec, float* bp) const;
  valarray<float> correct_ngram_hit_counts;
  valarray<float> hyp_ngram_counts;
  float ref_len;
  float hyp_len;
};

class BLEUScorerBase : public SentenceScorer {
 public:
  BLEUScorerBase(const vector<vector<WordID> >& references,
                 int n
             );
  ScoreP ScoreCandidate(const vector<WordID>& hyp) const;
  ScoreP ScoreCCandidate(const vector<WordID>& hyp) const;
  static ScoreP ScoreFromString(const string& in);

  virtual float ComputeRefLength(const vector<WordID>& hyp) const = 0;
 private:
  struct NGramCompare {
    int operator() (const vector<WordID>& a, const vector<WordID>& b) const {
      size_t as = a.size();
      size_t bs = b.size();
      const size_t s = (as < bs ? as : bs);
      for (size_t i = 0; i < s; ++i) {
         int d = a[i] - b[i];
         if (d < 0) return true;
	 if (d > 0) return false;
      }
      return as < bs;
    }
  };
  typedef map<vector<WordID>, pair<int,int>, NGramCompare> NGramCountMap;
  void CountRef(const vector<WordID>& ref) {
    NGramCountMap tc;
    vector<WordID> ngram(n_);
    int s = ref.size();
    for (int j=0; j<s; ++j) {
      int remaining = s-j;
      int k = (n_ < remaining ? n_ : remaining);
      ngram.clear();
      for (int i=1; i<=k; ++i) {
	ngram.push_back(ref[j + i - 1]);
        tc[ngram].first++;
      }
    }
    for (NGramCountMap::iterator i = tc.begin(); i != tc.end(); ++i) {
      pair<int,int>& p = ngrams_[i->first];
      if (p.first < i->second.first)
        p = i->second;
    }
  }

  void ComputeNgramStats(const vector<WordID>& sent,
			 valarray<float>* correct,
			 valarray<float>* hyp,
			 bool clip_counts)
    const {
    assert(correct->size() == n_);
    assert(hyp->size() == n_);
    vector<WordID> ngram(n_);
    (*correct) *= 0;
    (*hyp) *= 0;
    int s = sent.size();
    for (int j=0; j<s; ++j) {
      int remaining = s-j;
      int k = (n_ < remaining ? n_ : remaining);
      ngram.clear();
      for (int i=1; i<=k; ++i) {
	ngram.push_back(sent[j + i - 1]);
        pair<int,int>& p = ngrams_[ngram];
	if(clip_counts){
	  if (p.second < p.first) {
	    ++p.second;
	    (*correct)[i-1]++;
	  }}
	else {
	  ++p.second;
	  (*correct)[i-1]++;
	}
	// if the 1 gram isn't found, don't try to match don't need to match any 2- 3- .. grams:
	if (!p.first) {
	  for (; i<=k; ++i)
	    (*hyp)[i-1]++;
	} else {
          (*hyp)[i-1]++;
        }
      }
    }
  }

  mutable NGramCountMap ngrams_;
  int n_;
  vector<int> lengths_;
};

ScoreP BLEUScorerBase::ScoreFromString(const string& in) {
  istringstream is(in);
  int n;
  is >> n;
  BLEUScore* r = new BLEUScore(n);
  is >> r->ref_len >> r->hyp_len;

  for (int i = 0; i < n; ++i) {
    is >> r->correct_ngram_hit_counts[i];
    is >> r->hyp_ngram_counts[i];
  }
  return ScoreP(r);
}

class IBM_BLEUScorer : public BLEUScorerBase {
 public:
    IBM_BLEUScorer(const vector<vector<WordID> >& references,
		   int n=4) : BLEUScorerBase(references, n), lengths_(references.size()) {
   for (int i=0; i < references.size(); ++i)
     lengths_[i] = references[i].size();
 }
  float ComputeRefLength(const vector<WordID>& hyp) const {
    if (lengths_.size() == 1) return lengths_[0];
    int bestd = 2000000;
    int hl = hyp.size();
    int bl = -1;
    for (vector<int>::const_iterator ci = lengths_.begin(); ci != lengths_.end(); ++ci) {
      int cl = *ci;
      if (abs(cl - hl) < bestd) {
        bestd = abs(cl - hl);
        bl = cl;
      }
    }
    return bl;
  }
 private:
  vector<int> lengths_;
};

class NIST_BLEUScorer : public BLEUScorerBase {
 public:
    NIST_BLEUScorer(const vector<vector<WordID> >& references,
                    int n=4) : BLEUScorerBase(references, n),
		    shortest_(references[0].size()) {
   for (int i=1; i < references.size(); ++i)
     if (references[i].size() < shortest_)
       shortest_ = references[i].size();
 }
  float ComputeRefLength(const vector<WordID>& /* hyp */) const {
    return shortest_;
  }
 private:
  float shortest_;
};

class Koehn_BLEUScorer : public BLEUScorerBase {
 public:
    Koehn_BLEUScorer(const vector<vector<WordID> >& references,
                     int n=4) : BLEUScorerBase(references, n),
                     avg_(0) {
   for (int i=0; i < references.size(); ++i)
     avg_ += references[i].size();
   avg_ /= references.size();
 }
  float ComputeRefLength(const vector<WordID>& /* hyp */) const {
    return avg_;
  }
 private:
  float avg_;
};

ScorerP SentenceScorer::CreateSentenceScorer(const ScoreType type,
      const vector<vector<WordID> >& refs,
      const string& src)
{
  SentenceScorer *r=0;
  switch (type) {
  case IBM_BLEU: r = new IBM_BLEUScorer(refs, 4);break;
  case IBM_BLEU_3 : r = new IBM_BLEUScorer(refs,3);break;
    case NIST_BLEU: r = new NIST_BLEUScorer(refs, 4);break;
    case Koehn_BLEU: r = new Koehn_BLEUScorer(refs, 4);break;
    case AER: r = new AERScorer(refs, src);break;
    case TER: r = new TERScorer(refs);break;
    case SER: r = new SERScorer(refs);break;
    case BLEU_minus_TER_over_2: r = new BLEUTERCombinationScorer(refs);break;
    case METEOR: r = new ExternalSentenceScorer(ScoreServerManager::Instance("meteor"), refs); break;
    default:
      assert(!"Not implemented!");
  }
  return ScorerP(r);
}

ScoreP SentenceScorer::GetOne() const {
  Sentence s;
  return ScoreCCandidate(s)->GetOne();
}

ScoreP SentenceScorer::GetZero() const {
  Sentence s;
  return ScoreCCandidate(s)->GetZero();
}

ScoreP Score::GetOne(ScoreType type) {
  std::vector<SentenceScorer::Sentence > refs;
  return SentenceScorer::CreateSentenceScorer(type,refs)->GetOne();
}

ScoreP Score::GetZero(ScoreType type) {
  std::vector<SentenceScorer::Sentence > refs;
  return SentenceScorer::CreateSentenceScorer(type,refs)->GetZero();
}


ScoreP SentenceScorer::CreateScoreFromString(const ScoreType type, const string& in) {
  switch (type) {
    case IBM_BLEU:
  case IBM_BLEU_3:
    case NIST_BLEU:
    case Koehn_BLEU:
      return BLEUScorerBase::ScoreFromString(in);
    case TER:
      return TERScorer::ScoreFromString(in);
    case AER:
      return AERScorer::ScoreFromString(in);
    case SER:
      return SERScorer::ScoreFromString(in);
    case BLEU_minus_TER_over_2:
      return BLEUTERCombinationScorer::ScoreFromString(in);
    case METEOR:
      return ExternalSentenceScorer::ScoreFromString(ScoreServerManager::Instance("meteor"), in);
    default:
      assert(!"Not implemented!");
  }
}

void BLEUScore::ScoreDetails(string* details) const {
  char buf[2000];
  vector<float> precs(max(N(),4));
  float bp;
  float bleu = ComputeScore(&precs, &bp);
  for (int i=N();i<4;++i)
    precs[i]=0.;
  sprintf(buf, "BLEU = %.2f, %.1f|%.1f|%.1f|%.1f (brev=%.3f)",
       bleu*100.0,
       precs[0]*100.0,
       precs[1]*100.0,
       precs[2]*100.0,
       precs[3]*100.0,
       bp);
  *details = buf;
}

float BLEUScore::ComputeScore(vector<float>* precs, float* bp) const {
  float log_bleu = 0;
  if (precs) precs->clear();
  int count = 0;
  vector<float> total_precs(N());
  for (int i = 0; i < N(); ++i) {
    if (hyp_ngram_counts[i] > 0) {
      float cor_count = correct_ngram_hit_counts[i];
      // smooth bleu
      if (!cor_count) { cor_count = 0.01; }
      float lprec = log(cor_count) - log(hyp_ngram_counts[i]);
      if (precs) precs->push_back(exp(lprec));
      log_bleu += lprec;
      ++count;
    }
    total_precs[i] = log_bleu;
  }
  vector<float> bleus(N());
  float lbp = 0.0;
  if (hyp_len < ref_len)
    lbp = (hyp_len - ref_len) / hyp_len;
  log_bleu += lbp;
  if (bp) *bp = exp(lbp);
  float wb = 0;
  for (int i = 0; i < N(); ++i) {
    bleus[i] = exp(total_precs[i] / (i+1) + lbp);
    wb += bleus[i] / pow(2.0, 4.0 - i);
  }
  //return wb;
  return bleus.back();
}


//comptue scaled score for oracle retrieval
float BLEUScore::ComputePartialScore(vector<float>* precs, float* bp) const {
  // cerr << "Then here " << endl;
  float log_bleu = 0;
  if (precs) precs->clear();
  int count = 0;
  for (int i = 0; i < N(); ++i) {
    //  cerr << "In CPS " << hyp_ngram_counts[i] << " " << correct_ngram_hit_counts[i] << endl;
    if (hyp_ngram_counts[i] > 0) {
      float lprec = log(correct_ngram_hit_counts[i]) - log(hyp_ngram_counts[i]);
      if (precs) precs->push_back(exp(lprec));
      log_bleu += lprec;
      ++count;
    }
  }
  log_bleu /= static_cast<float>(count);
  float lbp = 0.0;
  if (hyp_len < ref_len)
    lbp = (hyp_len - ref_len) / hyp_len;
  log_bleu += lbp;
  if (bp) *bp = exp(lbp);
  return exp(log_bleu);
}

float BLEUScore::ComputePartialScore() const {
  // cerr << "In here first " << endl;
  return ComputePartialScore(NULL, NULL);
}

float BLEUScore::ComputeScore() const {
  return ComputeScore(NULL, NULL);
}

void BLEUScore::Subtract(const Score& rhs, Score* res) const {
  const BLEUScore& d = static_cast<const BLEUScore&>(rhs);
  BLEUScore* o = static_cast<BLEUScore*>(res);
  o->ref_len = ref_len - d.ref_len;
  o->hyp_len = hyp_len - d.hyp_len;
  o->correct_ngram_hit_counts = correct_ngram_hit_counts - d.correct_ngram_hit_counts;
  o->hyp_ngram_counts = hyp_ngram_counts - d.hyp_ngram_counts;
}

void BLEUScore::PlusEquals(const Score& delta) {
  const BLEUScore& d = static_cast<const BLEUScore&>(delta);
  correct_ngram_hit_counts += d.correct_ngram_hit_counts;
  hyp_ngram_counts += d.hyp_ngram_counts;
  ref_len += d.ref_len;
  hyp_len += d.hyp_len;
}

void BLEUScore::TimesEquals(float scale) {
  correct_ngram_hit_counts *= scale;
  hyp_ngram_counts *= scale;
  ref_len *= scale;
  hyp_len *= scale;
}

void BLEUScore::PlusEquals(const Score& delta, const float scale) {
  const BLEUScore& d = static_cast<const BLEUScore&>(delta);
  correct_ngram_hit_counts = correct_ngram_hit_counts + (d.correct_ngram_hit_counts * scale);
  hyp_ngram_counts = hyp_ngram_counts + (d.hyp_ngram_counts * scale);
  ref_len = ref_len + (d.ref_len * scale);
  hyp_len = hyp_len + (d.hyp_len * scale);
}

void BLEUScore::PlusPartialEquals(const Score& delta, int oracle_e_cover, int oracle_f_cover, int src_len){
  const BLEUScore& d = static_cast<const BLEUScore&>(delta);
  correct_ngram_hit_counts += d.correct_ngram_hit_counts;
  hyp_ngram_counts += d.hyp_ngram_counts;
  //scale the reference length according to the size of the input sentence covered by this rule

  ref_len *= (float)oracle_f_cover / src_len;
  ref_len += d.ref_len;

  hyp_len = oracle_e_cover;
  hyp_len += d.hyp_len;
}


ScoreP BLEUScore::GetZero() const {
  return ScoreP(new BLEUScore(N()));
}

ScoreP BLEUScore::GetOne() const {
  return ScoreP(new BLEUScore(N(),1));
}


void BLEUScore::Encode(string* out) const {
  ostringstream os;
  const int n = correct_ngram_hit_counts.size();
  os << n << ' ' << ref_len << ' ' << hyp_len;
  for (int i = 0; i < n; ++i)
    os << ' ' << correct_ngram_hit_counts[i] << ' ' << hyp_ngram_counts[i];
  *out = os.str();
}

BLEUScorerBase::BLEUScorerBase(const vector<vector<WordID> >& references,
                               int n) : SentenceScorer("BLEU"+boost::lexical_cast<string>(n),references),n_(n) {
  for (vector<vector<WordID> >::const_iterator ci = references.begin();
       ci != references.end(); ++ci) {
    lengths_.push_back(ci->size());
    CountRef(*ci);
  }
}

ScoreP BLEUScorerBase::ScoreCandidate(const vector<WordID>& hyp) const {
  BLEUScore* bs = new BLEUScore(n_);
  for (NGramCountMap::iterator i=ngrams_.begin(); i != ngrams_.end(); ++i)
    i->second.second = 0;
  ComputeNgramStats(hyp, &bs->correct_ngram_hit_counts, &bs->hyp_ngram_counts, true);
  bs->ref_len = ComputeRefLength(hyp);
  bs->hyp_len = hyp.size();
  return ScoreP(bs);
}

ScoreP BLEUScorerBase::ScoreCCandidate(const vector<WordID>& hyp) const {
  BLEUScore* bs = new BLEUScore(n_);
  for (NGramCountMap::iterator i=ngrams_.begin(); i != ngrams_.end(); ++i)
    i->second.second = 0;
  bool clip = false;
  ComputeNgramStats(hyp, &bs->correct_ngram_hit_counts, &bs->hyp_ngram_counts,clip);
  bs->ref_len = ComputeRefLength(hyp);
  bs->hyp_len = hyp.size();
  return ScoreP(bs);
}


DocScorer::~DocScorer() {
}

void DocScorer::Init(
      const ScoreType type,
      const vector<string>& ref_files,
      const string& src_file, bool verbose) {
  cerr << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"
          "!!! This code is using the deprecated DocScorer interface, please fix !!!\n"
          "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n";
  scorers_.clear();
  static const WordID kDIV = TD::Convert("|||");
  cerr << "Loading references (" << ref_files.size() << " files)\n";
  ReadFile srcrf;
  if (type == AER && src_file.size() > 0) {
    cerr << "  (source=" << src_file << ")\n";
    srcrf.Init(src_file);
  }
  std::vector<WordID> tmp;
  std::vector<ReadFile> ifs(ref_files.begin(),ref_files.end());
  for (int i=0; i < ref_files.size(); ++i) ifs[i].Init(ref_files[i]);
  char buf[64000];
  bool expect_eof = false;
  int line=0;
  while (ifs[0].get()) {
    vector<vector<WordID> > refs;
    for (int i=0; i < ref_files.size(); ++i) {
      istream &in=ifs[i].get();
      if (in.eof()) break;
      in.getline(buf, 64000);
      if (strlen(buf) == 0) {
        if (in.eof()) {
          if (!expect_eof) {
            assert(i == 0);
            expect_eof = true;
          }
          break;
        }
      } else { // process reference
        tmp.clear();
        TD::ConvertSentence(buf, &tmp);
        unsigned last = 0;
        for (unsigned j = 0; j < tmp.size(); ++j) {
          if (tmp[j] == kDIV) {
            refs.push_back(vector<WordID>(tmp.begin() + last, tmp.begin() + j));
            last = j + 1;
          }
        }
        refs.push_back(vector<WordID>(tmp.begin() + last, tmp.end()));
      }
      assert(!expect_eof);
    }
    if (!expect_eof) {
      string src_line;
      if (srcrf) {
        getline(srcrf.get(), src_line);
        map<string,string> dummy;
        ProcessAndStripSGML(&src_line, &dummy);
      }
      scorers_.push_back(ScorerP(SentenceScorer::CreateSentenceScorer(type, refs, src_line)));
      if (verbose)
        cerr<<"doc_scorer["<<line<<"] = "<<scorers_.back()->verbose_desc()<<endl;
      ++line;
    }
  }
  cerr << "Loaded reference translations for " << scorers_.size() << " sentences.\n";
}

DocStreamScorer::~DocStreamScorer() {
}

void DocStreamScorer::Init(
      const ScoreType type,
      const vector<string>& ref_files,
      const string& src_file, bool verbose) {
  scorers_.clear();
  // AER not supported in stream mode
  assert(type != AER);
  this->type = type;
  vector<vector<WordID> > refs(1);
  string src_line;
  // Initialize empty reference
  TD::ConvertSentence("", &refs[0]);
  scorer = ScorerP(SentenceScorer::CreateSentenceScorer(type, refs, src_line));
}

void DocStreamScorer::update(const std::string& ref) {
	vector<vector<WordID> > refs(1);
	string src_line;
	TD::ConvertSentence(ref, &refs[0]);
	scorer = ScorerP(SentenceScorer::CreateSentenceScorer(type, refs, src_line));
}
