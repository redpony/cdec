#include "scorer.h"

#include <map>
#include <sstream>
#include <iostream>
#include <fstream>
#include <cstdio>
#include <valarray>

#include <boost/shared_ptr.hpp>

#include "filelib.h"
#include "aligner.h"
#include "viterbi_envelope.h"
#include "error_surface.h"
#include "ter.h"
#include "aer_scorer.h"
#include "comb_scorer.h"
#include "tdict.h"
#include "stringlib.h"
#include "lattice.h"

using boost::shared_ptr;
using namespace std;

const bool minimize_segments = true;    // if adjacent segments have equal scores, merge them

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
  if (sl == "nist_bleu")
    return NIST_BLEU;
  if (sl == "koehn_bleu")
    return Koehn_BLEU;
  if (sl == "combi")
    return BLEU_minus_TER_over_2;
  cerr << "Don't understand score type '" << sl << "', defaulting to ibm_bleu.\n";
  return IBM_BLEU;
}

Score::~Score() {}
SentenceScorer::~SentenceScorer() {}
const std::string* SentenceScorer::GetSource() const { return NULL; }

class SERScore : public Score {
  friend class SERScorer;
 public:
  SERScore() : correct(0), total(0) {}
  float ComputeScore() const {
    return static_cast<float>(correct) / static_cast<float>(total);
  }
  void ScoreDetails(string* details) const {
    ostringstream os;
    os << "SER= " << ComputeScore() << " (" << correct << '/' << total << ')';
    *details = os.str();
  }
  void PlusEquals(const Score& delta) {
    correct += static_cast<const SERScore&>(delta).correct;
    total += static_cast<const SERScore&>(delta).total;
  }
  Score* GetZero() const { return new SERScore; }
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

class SERScorer : public SentenceScorer {
 public:
  SERScorer(const vector<vector<WordID> >& references) : refs_(references) {}
  Score* ScoreCandidate(const vector<WordID>& hyp) const {
    SERScore* res = new SERScore;
    res->total = 1;
    for (int i = 0; i < refs_.size(); ++i)
      if (refs_[i] == hyp) res->correct = 1;
    return res;
  }
  static Score* ScoreFromString(const string& data) {
    assert(!"Not implemented");
  }
 private:
  vector<vector<WordID> > refs_;
};

class BLEUScore : public Score {
  friend class BLEUScorerBase;
 public:
  BLEUScore(int n) : correct_ngram_hit_counts(0,n), hyp_ngram_counts(0,n) {
    ref_len = 0;
    hyp_len = 0; }
  float ComputeScore() const;
  void ScoreDetails(string* details) const;
  void PlusEquals(const Score& delta);
  Score* GetZero() const;
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
  float ComputeScore(vector<float>* precs, float* bp) const;
  valarray<int> correct_ngram_hit_counts;
  valarray<int> hyp_ngram_counts;
  float ref_len;
  int hyp_len;
};

class BLEUScorerBase : public SentenceScorer {
 public:
  BLEUScorerBase(const vector<vector<WordID> >& references,
             int n
             );
  Score* ScoreCandidate(const vector<WordID>& hyp) const;
  static Score* ScoreFromString(const string& in);

 protected:
  virtual float ComputeRefLength(const vector<WordID>& hyp) const = 0;
 private:
  struct NGramCompare {
    int operator() (const vector<WordID>& a, const vector<WordID>& b) {
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
       valarray<int>* correct,
       valarray<int>* hyp) const {
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
        if (p.second < p.first) {
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

Score* BLEUScorerBase::ScoreFromString(const string& in) {
  istringstream is(in);
  int n;
  is >> n;
  BLEUScore* r = new BLEUScore(n);
  is >> r->ref_len >> r->hyp_len;

  for (int i = 0; i < n; ++i) {
    is >> r->correct_ngram_hit_counts[i];
    is >> r->hyp_ngram_counts[i];
  }
  return r;
}

class IBM_BLEUScorer : public BLEUScorerBase {
 public:
    IBM_BLEUScorer(const vector<vector<WordID> >& references,
		   int n=4) : BLEUScorerBase(references, n), lengths_(references.size()) {
   for (int i=0; i < references.size(); ++i)
     lengths_[i] = references[i].size();
 }   
 protected:
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
 protected:
  float ComputeRefLength(const vector<WordID>& hyp) const {
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
 protected:
  float ComputeRefLength(const vector<WordID>& hyp) const {
    return avg_;
  }
 private:
  float avg_;
};

SentenceScorer* SentenceScorer::CreateSentenceScorer(const ScoreType type,
      const vector<vector<WordID> >& refs,
      const string& src) {
  switch (type) {
    case IBM_BLEU: return new IBM_BLEUScorer(refs, 4);
    case NIST_BLEU: return new NIST_BLEUScorer(refs, 4);
    case Koehn_BLEU: return new Koehn_BLEUScorer(refs, 4);
    case AER: return new AERScorer(refs, src);
    case TER: return new TERScorer(refs);
    case SER: return new SERScorer(refs);
    case BLEU_minus_TER_over_2: return new BLEUTERCombinationScorer(refs);
    default:
      assert(!"Not implemented!");
  }
}

Score* SentenceScorer::CreateScoreFromString(const ScoreType type, const string& in) {
  switch (type) {
    case IBM_BLEU:
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
    default:
      assert(!"Not implemented!");
  }
}

void SentenceScorer::ComputeErrorSurface(const ViterbiEnvelope& ve, ErrorSurface* env, const ScoreType type, const Hypergraph& hg) const {
  vector<WordID> prev_trans;
  const vector<shared_ptr<Segment> >& ienv = ve.GetSortedSegs();
  env->resize(ienv.size());
  Score* prev_score = NULL;
  int j = 0;
  for (int i = 0; i < ienv.size(); ++i) {
    const Segment& seg = *ienv[i];
    vector<WordID> trans;
    if (type == AER) {
      vector<bool> edges(hg.edges_.size(), false);
      seg.CollectEdgesUsed(&edges);  // get the set of edges in the viterbi
                                     // alignment
      ostringstream os;
      const string* psrc = this->GetSource();
      if (psrc == NULL) {
        cerr << "AER scoring in VEST requires source, but it is missing!\n";
        abort();
      }
      size_t pos = psrc->rfind(" ||| ");
      if (pos == string::npos) {
        cerr << "Malformed source for AER: expected |||\nINPUT: " << *psrc << endl;
        abort();
      }
      Lattice src;
      Lattice ref;
      LatticeTools::ConvertTextOrPLF(psrc->substr(0, pos), &src);
      LatticeTools::ConvertTextOrPLF(psrc->substr(pos + 5), &ref);
      AlignerTools::WriteAlignment(src, ref, hg, &os, true, &edges);
      string tstr = os.str();
      TD::ConvertSentence(tstr.substr(tstr.rfind(" ||| ") + 5), &trans);
    } else {
      seg.ConstructTranslation(&trans);
    }
    // cerr << "Scoring: " << TD::GetString(trans) << endl;
    if (trans == prev_trans) {
      if (!minimize_segments) {
        assert(prev_score); // if this fails, it means
	                    // the decoder can generate null translations
        ErrorSegment& out = (*env)[j];
        out.delta = prev_score->GetZero();
        out.x = seg.x;
	++j;
      }
      // cerr << "Identical translation, skipping scoring\n";
    } else {
      Score* score = ScoreCandidate(trans);
      // cerr << "score= " << score->ComputeScore() << "\n";
      Score* cur_delta = score->GetZero();
      // just record the score diffs
      if (!prev_score)
        prev_score = score->GetZero();

      score->Subtract(*prev_score, cur_delta);
      delete prev_score;
      prev_trans.swap(trans);
      prev_score = score;
      if ((!minimize_segments) || (!cur_delta->IsAdditiveIdentity())) {
        ErrorSegment& out = (*env)[j];
	out.delta = cur_delta;
        out.x = seg.x;
	++j;
      }
    }
  }
  delete prev_score;
  // cerr << " In segments: " << ienv.size() << endl;
  // cerr << "Out segments: " << j << endl;
  assert(j > 0);
  env->resize(j);
}

void BLEUScore::ScoreDetails(string* details) const {
  char buf[2000];
  vector<float> precs(4);
  float bp;
  float bleu = ComputeScore(&precs, &bp);
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
  for (int i = 0; i < hyp_ngram_counts.size(); ++i) {
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

Score* BLEUScore::GetZero() const {
  return new BLEUScore(hyp_ngram_counts.size());
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
                       int n) : n_(n) {
  for (vector<vector<WordID> >::const_iterator ci = references.begin();
       ci != references.end(); ++ci) {
    lengths_.push_back(ci->size());
    CountRef(*ci);
  }
}
 
Score* BLEUScorerBase::ScoreCandidate(const vector<WordID>& hyp) const {
  BLEUScore* bs = new BLEUScore(n_);
  for (NGramCountMap::iterator i=ngrams_.begin(); i != ngrams_.end(); ++i)
    i->second.second = 0;
  ComputeNgramStats(hyp, &bs->correct_ngram_hit_counts, &bs->hyp_ngram_counts);
  bs->ref_len = ComputeRefLength(hyp);
  bs->hyp_len = hyp.size();
  return bs;
}

DocScorer::~DocScorer() {
  for (int i=0; i < scorers_.size(); ++i)
    delete scorers_[i];
}

DocScorer::DocScorer(
      const ScoreType type,
      const vector<string>& ref_files,
      const string& src_file) {
  // TODO stop using valarray, start using ReadFile
  cerr << "Loading references (" << ref_files.size() << " files)\n";
  shared_ptr<ReadFile> srcrf;
  if (type == AER && src_file.size() > 0) {
    cerr << "  (source=" << src_file << ")\n";
    srcrf.reset(new ReadFile(src_file));
  }
  valarray<ifstream> ifs(ref_files.size());
  for (int i=0; i < ref_files.size(); ++i) {
     ifs[i].open(ref_files[i].c_str());
     assert(ifs[i].good());
  }
  char buf[64000];
  bool expect_eof = false;
  while (!ifs[0].eof()) {
    vector<vector<WordID> > refs(ref_files.size());
    for (int i=0; i < ref_files.size(); ++i) {
      if (ifs[i].eof()) break;
      ifs[i].getline(buf, 64000);
      refs[i].clear();
      if (strlen(buf) == 0) {
        if (ifs[i].eof()) {
	  if (!expect_eof) {
	    assert(i == 0);
	    expect_eof = true;
	  } 
          break;
	}
      } else {
        TD::ConvertSentence(buf, &refs[i]);
        assert(!refs[i].empty());
      }
      assert(!expect_eof);
    }
    if (!expect_eof) {
      string src_line;
      if (srcrf) {
        getline(*srcrf->stream(), src_line);
        map<string,string> dummy;
        ProcessAndStripSGML(&src_line, &dummy);
      }
      scorers_.push_back(SentenceScorer::CreateSentenceScorer(type, refs, src_line));
    }
  }
  cerr << "Loaded reference translations for " << scorers_.size() << " sentences.\n";
}

