/*
 * Featurize a grammar in striped format
 */
#include <iostream>
#include <sstream>
#include <string>
#include <map>
#include <vector>
#include <utility>
#include <cstdlib>
#include <tr1/unordered_map>

#include "lex_trans_tbl.h"
#include "sparse_vector.h"
#include "sentence_pair.h"
#include "extract.h"
#include "fdict.h"
#include "tdict.h"
#include "filelib.h"
#include "striped_grammar.h"

#include <boost/tuple/tuple.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/functional/hash.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

using namespace std;
using namespace std::tr1;
using boost::shared_ptr;
namespace po = boost::program_options;

static string aligned_corpus;
static const size_t MAX_LINE_LENGTH = 64000000;

// Data structures for indexing and counting rules
//typedef boost::tuple< WordID, vector<WordID>, vector<WordID> > RuleTuple;
struct RuleTuple {
  RuleTuple(const WordID& lhs, const vector<WordID>& s, const vector<WordID>& t)
  : m_lhs(lhs), m_source(s), m_target(t) {
    hash_value();
    m_dirty = false;
  }

  size_t hash_value() const {
//    if (m_dirty) {
      size_t hash = 0;
      boost::hash_combine(hash, m_lhs);
      boost::hash_combine(hash, m_source);
      boost::hash_combine(hash, m_target);
//    }
//    m_dirty = false;
    return hash;
  }

  bool operator==(RuleTuple const& b) const
  { return m_lhs == b.m_lhs && m_source == b.m_source && m_target == b.m_target; }

  WordID& lhs() { m_dirty=true; return m_lhs; }
  vector<WordID>& source() { m_dirty=true; return m_source; }
  vector<WordID>& target() { m_dirty=true; return m_target; }
  const WordID& lhs() const { return m_lhs; }
  const vector<WordID>& source() const { return m_source; }
  const vector<WordID>& target() const { return m_target; }

//  mutable size_t m_hash;
private:
  WordID m_lhs;
  vector<WordID> m_source, m_target;
  mutable bool m_dirty;
};
std::size_t hash_value(RuleTuple const& b) { return b.hash_value(); }
bool operator<(RuleTuple const& l, RuleTuple const& r) {
  if (l.lhs() < r.lhs()) return true;
  else if (l.lhs() == r.lhs()) {
    if (l.source() < r.source()) return true;
    else if (l.source() == r.source()) {
      if (l.target() < r.target()) return true;
    }
  }
  return false;
}

ostream& operator<<(ostream& o, RuleTuple const& r) {
  o << "(" << r.lhs() << "-->" << "<";
  for (vector<WordID>::const_iterator it=r.source().begin(); it!=r.source().end(); ++it)
    o << TD::Convert(*it) << " ";
  o << "|||";
  for (vector<WordID>::const_iterator it=r.target().begin(); it!=r.target().end(); ++it)
    o << " " << TD::Convert(*it);
  o << ">)";
  return o;
}

template <typename Key>
struct FreqCount {
  //typedef unordered_map<Key, int, boost::hash<Key> > Counts;
  typedef map<Key, int> Counts;
  Counts counts;

  int inc(const Key& r, int c=1) {
    pair<typename Counts::iterator,bool> itb
      = counts.insert(make_pair(r,c));
    if (!itb.second)
      itb.first->second += c;
    return itb.first->second;
  }

  int inc_if_exists(const Key& r, int c=1) {
    typename Counts::iterator it = counts.find(r);
    if (it != counts.end())
      it->second += c;
    return it->second;
  }

  int count(const Key& r) const {
    typename Counts::const_iterator it = counts.find(r);
    if (it == counts.end()) return 0;
    return it->second;
  }

  int operator()(const Key& r) const { return count(r); }
};
typedef FreqCount<RuleTuple> RuleFreqCount;

class FeatureExtractor;
class FERegistry;
struct FEFactoryBase {
  virtual ~FEFactoryBase() {}
  virtual boost::shared_ptr<FeatureExtractor> Create() const = 0;
};


class FERegistry {
  friend class FEFactoryBase;
 public:
  FERegistry() {}
  boost::shared_ptr<FeatureExtractor> Create(const std::string& ffname) const {
    map<string, boost::shared_ptr<FEFactoryBase> >::const_iterator it = reg_.find(ffname);
    boost::shared_ptr<FeatureExtractor> res;
    if (it == reg_.end()) {
      cerr << "I don't know how to create feature " << ffname << endl;
    } else {
      res = it->second->Create();
    }
    return res;
  }
  void DisplayList(ostream* out) const {
    bool first = true;
    for (map<string, boost::shared_ptr<FEFactoryBase> >::const_iterator it = reg_.begin();
        it != reg_.end(); ++it) {
      if (first) {first=false;} else {*out << ' ';}
      *out << it->first;
    }
  }

  void Register(const std::string& ffname, FEFactoryBase* factory) {
    if (reg_.find(ffname) != reg_.end()) {
      cerr << "Duplicate registration of FeatureExtractor with name " << ffname << "!\n";
      exit(1);
    }
    reg_[ffname].reset(factory);
  }

 private:
  std::map<std::string, boost::shared_ptr<FEFactoryBase> > reg_;
};

template<class FE>
class FEFactory : public FEFactoryBase {
  boost::shared_ptr<FeatureExtractor> Create() const {
    return boost::shared_ptr<FeatureExtractor>(new FE);
  }
};

void InitCommandLine(const FERegistry& r, int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  ostringstream feats;
  feats << "[multiple] Features to extract (";
  r.DisplayList(&feats);
  feats << ")";
  opts.add_options()
        ("filtered_grammar,g", po::value<string>(), "Grammar to add features to")
        ("list_features,L", "List extractable features")
        ("feature,f", po::value<vector<string> >()->composing(), feats.str().c_str())
        ("aligned_corpus,c", po::value<string>(), "Aligned corpus (single line format)")
        ("help,h", "Print this help message and exit");
  po::options_description clo("Command line options");
  po::options_description dcmdline_options;
  dcmdline_options.add(opts);

  po::store(parse_command_line(argc, argv, dcmdline_options), *conf);
  po::notify(*conf);

  if (conf->count("help") || conf->count("aligned_corpus")==0 || conf->count("feature") == 0) {
    cerr << "\nUsage: featurize_grammar -g FILTERED-GRAMMAR.gz -c ALIGNED_CORPUS.fr-en-al -f Feat1 -f Feat2 ... < UNFILTERED-GRAMMAR\n";
    cerr << dcmdline_options << endl;
    exit(1);
  }
}

static const bool DEBUG = false;

void LexTranslationTable::createTTable(const char* buf){
  AnnotatedParallelSentence sent;
  sent.ParseInputLine(buf);

  //iterate over the alignment to compute aligned words

  for(int i =0;i<sent.aligned.width();i++)
    {
      for (int j=0;j<sent.aligned.height();j++)
        {
          if (DEBUG) cerr << sent.aligned(i,j) << " ";
          if( sent.aligned(i,j))
            {
              if (DEBUG) cerr << TD::Convert(sent.f[i])  << " aligned to " << TD::Convert(sent.e[j]);
              ++word_translation[pair<WordID,WordID> (sent.f[i], sent.e[j])];
              ++total_foreign[sent.f[i]];
              ++total_english[sent.e[j]];
            }
        }
      if (DEBUG)  cerr << endl;
    }
  if (DEBUG) cerr << endl;

  const WordID NULL_ = TD::Convert("NULL");
  //handle unaligned words - align them to null
  for (int j =0; j < sent.e_len; j++) {
    if (sent.e_aligned[j]) continue;
    ++word_translation[pair<WordID,WordID> (NULL_, sent.e[j])];
    ++total_foreign[NULL_];
    ++total_english[sent.e[j]];
  }

  for (int i =0; i < sent.f_len; i++) {
    if (sent.f_aligned[i]) continue;
    ++word_translation[pair<WordID,WordID> (sent.f[i], NULL_)];
    ++total_english[NULL_];
    ++total_foreign[sent.f[i]];
  }
}

inline float safenlog(float v) {
  if (v == 1.0f) return 0.0f;
  float res = -log(v);
  if (res > 100.0f) res = 100.0f;
  return res;
}

static bool IsZero(float f) { return (f > 0.999 && f < 1.001); }

struct FeatureExtractor {
  // create any keys necessary
  virtual void ObserveFilteredRule(const WordID /* lhs */,
                                   const vector<WordID>& /* src */,
                                   const vector<WordID>& /* trg */) {}

  // compute statistics over keys, the same lhs-src-trg tuple may be seen
  // more than once
  virtual void ObserveUnfilteredRule(const WordID /* lhs */,
                                     const vector<WordID>& /* src */,
                                     const vector<WordID>& /* trg */,
                                     const RuleStatistics& /* info */) {}

  // compute features, a unique lhs-src-trg tuple will be seen exactly once
  virtual void ExtractFeatures(const WordID lhs,
                               const vector<WordID>& src,
                               const vector<WordID>& trg,
                               const RuleStatistics& info,
                               SparseVector<float>* result) const = 0;

  virtual ~FeatureExtractor() {}
};

struct LogRuleCount : public FeatureExtractor {
  LogRuleCount() :
    fid_(FD::Convert("LogRuleCount")),
    sfid_(FD::Convert("SingletonRule")),
    kCFE(FD::Convert("CFE")) {}
  virtual void ExtractFeatures(const WordID lhs,
                               const vector<WordID>& src,
                               const vector<WordID>& trg,
                               const RuleStatistics& info,
                               SparseVector<float>* result) const {
    (void) lhs; (void) src; (void) trg;
    //result->set_value(fid_, log(info.counts.get(kCFE)));
    result->set_value(fid_, log(info.counts.get(kCFE)));
    if (IsZero(info.counts.get(kCFE)))
      result->set_value(sfid_, 1);
  }
  const int fid_;
  const int sfid_;
  const int kCFE;
};

struct RulePenalty : public FeatureExtractor {
  RulePenalty() : fid_(FD::Convert("RulePenalty")) {}
  virtual void ExtractFeatures(const WordID /*lhs*/,
                               const vector<WordID>& /*src*/,
                               const vector<WordID>& /*trg*/,
                               const RuleStatistics& /*info*/,
                               SparseVector<float>* result) const
  { result->set_value(fid_, 1); }

  const int fid_;
};

// The negative log of the condition rule probs
// ignoring the identities of the  non-terminals.
// i.e. the prob Hiero would assign.
// Also extracts Labelled features.
struct XFeatures: public FeatureExtractor {
  XFeatures() :
    fid_xfe(FD::Convert("XFE")),
    fid_xef(FD::Convert("XEF")),
    fid_labelledfe(FD::Convert("LabelledFE")),
    fid_labelledef(FD::Convert("LabelledEF")),
    fid_xesingleton(FD::Convert("XE_Singleton")),
    fid_xfsingleton(FD::Convert("XF_Singleton")),
    kCFE(FD::Convert("CFE")) {}
  virtual void ObserveFilteredRule(const WordID /*lhs*/,
                                   const vector<WordID>& src,
                                   const vector<WordID>& trg) {
    RuleTuple r(-1, src, trg);
    map_rule(r);
    rule_counts.inc(r, 0);
    source_counts.inc(r.source(), 0);
    target_counts.inc(r.target(), 0);
  }

  virtual void ObserveUnfilteredRule(const WordID /*lhs*/,
                                     const vector<WordID>& src,
                                     const vector<WordID>& trg,
                                     const RuleStatistics& info) {
    RuleTuple r(-1, src, trg);
    map_rule(r);
    const int count = info.counts.get(kCFE);
    assert(count > 0);
    rule_counts.inc_if_exists(r, count);
    source_counts.inc_if_exists(r.source(), count);
    target_counts.inc_if_exists(r.target(), count);
  }

  virtual void ExtractFeatures(const WordID /*lhs*/,
                               const vector<WordID>& src,
                               const vector<WordID>& trg,
                               const RuleStatistics& info,
                               SparseVector<float>* result) const {
    RuleTuple r(-1, src, trg);
    map_rule(r);
    double l_r_freq = log(rule_counts(r));

    const int t_c = target_counts(r.target());
    assert(t_c > 0);
    result->set_value(fid_xfe, log(t_c) - l_r_freq);
    result->set_value(fid_labelledfe, log(t_c) - log(info.counts.get(kCFE)));
//    if (t_c == 1)
//      result->set_value(fid_xesingleton, 1.0);

    const int s_c = source_counts(r.source());
    assert(s_c > 0);
    result->set_value(fid_xef, log(s_c) - l_r_freq);
    result->set_value(fid_labelledef, log(s_c) - log(info.counts.get(kCFE)));
//    if (s_c == 1)
//      result->set_value(fid_xfsingleton, 1.0);
  }

  void map_rule(RuleTuple& r) const {
    vector<WordID> indexes; int i=0;
    for (vector<WordID>::iterator it = r.target().begin(); it != r.target().end(); ++it) {
      if (*it <= 0)
        indexes.push_back(*it);
    }
    for (vector<WordID>::iterator it = r.source().begin(); it != r.source().end(); ++it) {
      if (*it <= 0)
        *it = indexes.at(i++);
    }
  }

  const int fid_xfe, fid_xef;
  const int fid_labelledfe, fid_labelledef;
  const int fid_xesingleton, fid_xfsingleton;
  const int kCFE;
  RuleFreqCount rule_counts;
  FreqCount< vector<WordID> > source_counts, target_counts;
};


struct LabelledRuleConditionals: public FeatureExtractor {
  LabelledRuleConditionals() :
    fid_fe(FD::Convert("LabelledFE")),
    fid_ef(FD::Convert("LabelledEF")),
    kCFE(FD::Convert("CFE")) {}
  virtual void ObserveFilteredRule(const WordID lhs,
                                   const vector<WordID>& src,
                                   const vector<WordID>& trg) {
    RuleTuple r(lhs, src, trg);
    rule_counts.inc(r, 0);
    source_counts.inc(r.source(), 0);

    target_counts.inc(r.target(), 0);
  }

  virtual void ObserveUnfilteredRule(const WordID lhs,
                                     const vector<WordID>& src,
                                     const vector<WordID>& trg,
                                     const RuleStatistics& info) {
    RuleTuple r(lhs, src, trg);
    rule_counts.inc_if_exists(r, info.counts.get(kCFE));
    source_counts.inc_if_exists(r.source(), info.counts.get(kCFE));

    target_counts.inc_if_exists(r.target(), info.counts.get(kCFE));
  }

  virtual void ExtractFeatures(const WordID lhs,
                               const vector<WordID>& src,
                               const vector<WordID>& trg,
                               const RuleStatistics& /*info*/,
                               SparseVector<float>* result) const {
    RuleTuple r(lhs, src, trg);
    double l_r_freq = log(rule_counts(r));
    result->set_value(fid_fe, log(target_counts(r.target())) - l_r_freq);
    result->set_value(fid_ef, log(source_counts(r.source())) - l_r_freq);
  }

  const int fid_fe, fid_ef;
  const int kCFE;
  RuleFreqCount rule_counts;
  FreqCount< vector<WordID> > source_counts, target_counts;
};

struct LHSProb: public FeatureExtractor {
  LHSProb() : fid_(FD::Convert("LHSProb")), kCFE(FD::Convert("CFE")), total_count(0) {}

  virtual void ObserveUnfilteredRule(const WordID lhs,
                                     const vector<WordID>& /*src*/,
                                     const vector<WordID>& /*trg*/,
                                     const RuleStatistics& info) {
    int count = info.counts.get(kCFE);
    total_count += count;
    lhs_counts.inc(lhs, count);
  }

  virtual void ExtractFeatures(const WordID lhs,
                               const vector<WordID>& /*src*/,
                               const vector<WordID>& /*trg*/,
                               const RuleStatistics& /*info*/,
                               SparseVector<float>* result) const {
    double lhs_log_prob =  log(total_count) - log(lhs_counts(lhs));
    result->set_value(fid_, lhs_log_prob);
  }

  const int fid_;
  const int kCFE;
  int total_count;
  FreqCount<WordID> lhs_counts;
};

// Proper rule generative probability: p( s,t | lhs)
struct GenerativeProb: public FeatureExtractor {
  GenerativeProb() :
    fid_(FD::Convert("GenerativeProb")),
    kCFE(FD::Convert("CFE")) {}

  virtual void ObserveUnfilteredRule(const WordID lhs,
                                     const vector<WordID>& /*src*/,
                                     const vector<WordID>& /*trg*/,
                                     const RuleStatistics& info)
  { lhs_counts.inc(lhs, info.counts.get(kCFE)); }

  virtual void ExtractFeatures(const WordID lhs,
                               const vector<WordID>& /*src*/,
                               const vector<WordID>& /*trg*/,
                               const RuleStatistics& info,
                               SparseVector<float>* result) const {
    double log_prob = log(lhs_counts(lhs)) - log(info.counts.get(kCFE));
    result->set_value(fid_, log_prob);
  }

  const int fid_;
  const int kCFE;
  FreqCount<WordID> lhs_counts;
};

// remove terminals from the rules before estimating the conditional prob
struct LabellingShape: public FeatureExtractor {
  LabellingShape() : fid_(FD::Convert("LabellingShape")), kCFE(FD::Convert("CFE")) {}

  virtual void ObserveFilteredRule(const WordID /*lhs*/,
                                   const vector<WordID>& src,
                                   const vector<WordID>& trg) {
    RuleTuple r(-1, src, trg);
    map_rule(r);
    rule_counts.inc(r, 0);
    source_counts.inc(r.source(), 0);
  }

  virtual void ObserveUnfilteredRule(const WordID /*lhs*/,
                                     const vector<WordID>& src,
                                     const vector<WordID>& trg,
                                     const RuleStatistics& info) {
    RuleTuple r(-1, src, trg);
    map_rule(r);
    rule_counts.inc_if_exists(r, info.counts.get(kCFE));
    source_counts.inc_if_exists(r.source(), info.counts.get(kCFE));
  }

  virtual void ExtractFeatures(const WordID /*lhs*/,
                               const vector<WordID>& src,
                               const vector<WordID>& trg,
                               const RuleStatistics& /*info*/,
                               SparseVector<float>* result) const {
    RuleTuple r(-1, src, trg);
    map_rule(r);
    double l_r_freq = log(rule_counts(r));
    result->set_value(fid_, log(source_counts(r.source())) - l_r_freq);
  }

  // Replace all terminals with generic -1
  void map_rule(RuleTuple& r) const {
    for (vector<WordID>::iterator it = r.target().begin(); it != r.target().end(); ++it)
      if (*it <= 0) *it = -1;
    for (vector<WordID>::iterator it = r.source().begin(); it != r.source().end(); ++it)
      if (*it <= 0) *it = -1;
  }

  const int fid_, kCFE;
  RuleFreqCount rule_counts;
  FreqCount< vector<WordID> > source_counts;
};


// this extracts the lexical translation prob features
// in BOTH directions.
struct LexProbExtractor : public FeatureExtractor {
  LexProbExtractor() :
      e2f_(FD::Convert("LexE2F")), f2e_(FD::Convert("LexF2E")) {
    ReadFile rf(aligned_corpus);
    //create lexical translation table
    cerr << "Computing lexical translation probabilities from " << aligned_corpus << "..." << endl;
    char* buf = new char[MAX_LINE_LENGTH];
    istream& alignment = *rf.stream();
    while(alignment) {
      alignment.getline(buf, MAX_LINE_LENGTH);
      if (buf[0] == 0) continue;
      table.createTTable(buf);
    }
    delete[] buf;
  }

  virtual void ExtractFeatures(const WordID /*lhs*/,
                               const vector<WordID>& src,
                               const vector<WordID>& trg,
                               const RuleStatistics& info,
                               SparseVector<float>* result) const {
    map <WordID, pair<int, float> > foreign_aligned;
    map <WordID, pair<int, float> > english_aligned;

    //Loop over all the alignment points to compute lexical translation probability
    const vector< pair<short,short> >& al = info.aligns;
    vector< pair<short,short> >::const_iterator ita;
    for (ita = al.begin(); ita != al.end(); ++ita) {
            if (DEBUG) {
              cerr << "\nA:" << ita->first << "," << ita->second << "::";
              cerr <<  TD::Convert(src[ita->first]) << "-" << TD::Convert(trg[ita->second]);
            }

            //Lookup this alignment probability in the table
            int temp = table.word_translation[pair<WordID,WordID> (src[ita->first],trg[ita->second])];
            float f2e=0, e2f=0;
            if ( table.total_foreign[src[ita->first]] != 0)
              f2e = (float) temp / table.total_foreign[src[ita->first]];
            if ( table.total_english[trg[ita->second]] !=0 )
              e2f = (float) temp / table.total_english[trg[ita->second]];
            if (DEBUG) printf (" %d %E %E\n", temp, f2e, e2f);

            //local counts to keep track of which things haven't been aligned, to later compute their null alignment
            if (foreign_aligned.count(src[ita->first])) {
              foreign_aligned[ src[ita->first] ].first++;
              foreign_aligned[ src[ita->first] ].second += e2f;
            } else {
              foreign_aligned[ src[ita->first] ] = pair<int,float> (1,e2f);
            }

            if (english_aligned.count( trg[ ita->second] )) {
               english_aligned[ trg[ ita->second] ].first++;
               english_aligned[ trg[ ita->second] ].second += f2e;
            } else {
               english_aligned[ trg[ ita->second] ] = pair<int,float> (1,f2e);
            }
          }

          float final_lex_f2e=1, final_lex_e2f=1;
          static const WordID NULL_ = TD::Convert("NULL");

          //compute lexical weight P(F|E) and include unaligned foreign words
           for(int i=0;i<src.size(); i++) {
               if (!table.total_foreign.count(src[i])) continue;      //if we dont have it in the translation table, we won't know its lexical weight

               if (foreign_aligned.count(src[i]))
                 {
                   pair<int, float> temp_lex_prob = foreign_aligned[src[i]];
                   final_lex_e2f *= temp_lex_prob.second / temp_lex_prob.first;
                 }
               else //dealing with null alignment
                 {
                   int temp_count = table.word_translation[pair<WordID,WordID> (src[i],NULL_)];
                   float temp_e2f = (float) temp_count / table.total_english[NULL_];
                   final_lex_e2f *= temp_e2f;
                 }

             }

           //compute P(E|F) unaligned english words
           for(int j=0; j< trg.size(); j++) {
               if (!table.total_english.count(trg[j])) continue;

               if (english_aligned.count(trg[j]))
                 {
                   pair<int, float> temp_lex_prob = english_aligned[trg[j]];
                   final_lex_f2e *= temp_lex_prob.second / temp_lex_prob.first;
                 }
               else //dealing with null
                 {
                   int temp_count = table.word_translation[pair<WordID,WordID> (NULL_,trg[j])];
                   float temp_f2e = (float) temp_count / table.total_foreign[NULL_];
                   final_lex_f2e *= temp_f2e;
                 }
           }
     result->set_value(e2f_, safenlog(final_lex_e2f));
     result->set_value(f2e_, safenlog(final_lex_f2e));
  }
  const int e2f_, f2e_;
  mutable LexTranslationTable table;
};

struct Featurizer {
  Featurizer(const vector<boost::shared_ptr<FeatureExtractor> >& ex) : extractors(ex) {
  }
  void Callback1(WordID lhs, const vector<WordID>& src, const ID2RuleStatistics& trgs) {
    for (ID2RuleStatistics::const_iterator it = trgs.begin(); it != trgs.end(); ++it) {
      for (int i = 0; i < extractors.size(); ++i)
        extractors[i]->ObserveFilteredRule(lhs, src, it->first);
    }
  }
  void Callback2(WordID lhs, const vector<WordID>& src, const ID2RuleStatistics& trgs) {
    for (ID2RuleStatistics::const_iterator it = trgs.begin(); it != trgs.end(); ++it) {
      for (int i = 0; i < extractors.size(); ++i)
        extractors[i]->ObserveUnfilteredRule(lhs, src, it->first, it->second);
    }
  }
  void Callback3(WordID lhs, const vector<WordID>& src, const ID2RuleStatistics& trgs) {
    for (ID2RuleStatistics::const_iterator it = trgs.begin(); it != trgs.end(); ++it) {
      SparseVector<float> feats;
      for (int i = 0; i < extractors.size(); ++i)
        extractors[i]->ExtractFeatures(lhs, src, it->first, it->second, &feats);
      cout << '[' << TD::Convert(-lhs) << "] ||| ";
      WriteNamed(src, &cout);
      cout << " ||| ";
      WriteAnonymous(it->first, &cout);
      cout << " ||| ";
      print(cout,feats,"=");
      cout << endl;
    }
  }
 private:
  vector<boost::shared_ptr<FeatureExtractor> > extractors;
};

void cb1(WordID lhs, const vector<WordID>& src_rhs, const ID2RuleStatistics& rules, void* extra) {
  static_cast<Featurizer*>(extra)->Callback1(lhs, src_rhs, rules);
}

void cb2(WordID lhs, const vector<WordID>& src_rhs, const ID2RuleStatistics& rules, void* extra) {
  static_cast<Featurizer*>(extra)->Callback2(lhs, src_rhs, rules);
}

void cb3(WordID lhs, const vector<WordID>& src_rhs, const ID2RuleStatistics& rules, void* extra) {
  static_cast<Featurizer*>(extra)->Callback3(lhs, src_rhs, rules);
}

int main(int argc, char** argv){
  FERegistry reg;
  reg.Register("LogRuleCount", new FEFactory<LogRuleCount>);
  reg.Register("LexProb", new FEFactory<LexProbExtractor>);
  reg.Register("XFeatures", new FEFactory<XFeatures>);
  reg.Register("LabelledRuleConditionals", new FEFactory<LabelledRuleConditionals>);
  reg.Register("RulePenalty", new FEFactory<RulePenalty>);
  reg.Register("LHSProb", new FEFactory<LHSProb>);
  reg.Register("LabellingShape", new FEFactory<LabellingShape>);
  reg.Register("GenerativeProb", new FEFactory<GenerativeProb>);
  po::variables_map conf;
  InitCommandLine(reg, argc, argv, &conf);
  aligned_corpus = conf["aligned_corpus"].as<string>();  // GLOBAL VAR
  ReadFile fg1(conf["filtered_grammar"].as<string>());

  vector<string> feats = conf["feature"].as<vector<string> >();
  vector<boost::shared_ptr<FeatureExtractor> > extractors(feats.size());
  for (int i = 0; i < feats.size(); ++i)
    extractors[i] = reg.Create(feats[i]);
  Featurizer fizer(extractors);

  cerr << "Reading filtered grammar to detect keys..." << endl;
  StripedGrammarLexer::ReadStripedGrammar(fg1.stream(), cb1, &fizer);

  cerr << "Reading unfiltered grammar..." << endl;
  StripedGrammarLexer::ReadStripedGrammar(&cin, cb2, &fizer);

  ReadFile fg2(conf["filtered_grammar"].as<string>());
  cerr << "Reading filtered grammar and adding features..." << endl;
  StripedGrammarLexer::ReadStripedGrammar(fg2.stream(), cb3, &fizer);

  return 0;
}

