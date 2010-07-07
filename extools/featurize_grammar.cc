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
#include <fstream>
#include <tr1/unordered_map>
#include <boost/regex.hpp>

#include "suffix_tree.h"
#include "sparse_vector.h"
#include "sentence_pair.h"
#include "extract.h"
#include "fdict.h"
#include "tdict.h"
#include "lex_trans_tbl.h"
#include "filelib.h"

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

typedef unordered_map<vector<WordID>, RuleStatistics, boost::hash<vector<WordID> > > ID2RuleStatistics;

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

bool validate_non_terminal(const std::string& s)
{
  static const boost::regex r("\\[X\\d+,\\d+\\]|\\[\\d+\\]");
  return regex_match(s, r);
}

namespace {
  inline bool IsWhitespace(char c) { return c == ' ' || c == '\t'; }
  inline bool IsBracket(char c){return c == '[' || c == ']';}
  inline void SkipWhitespace(const char* buf, int* ptr) {
    while (buf[*ptr] && IsWhitespace(buf[*ptr])) { ++(*ptr); }
  }
}


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
    map<string, shared_ptr<FEFactoryBase> >::const_iterator it = reg_.find(ffname);
    shared_ptr<FeatureExtractor> res;
    if (it == reg_.end()) {
      cerr << "I don't know how to create feature " << ffname << endl;
    } else {
      res = it->second->Create();
    }
    return res;
  }
  void DisplayList(ostream* out) const {
    bool first = true;
    for (map<string, shared_ptr<FEFactoryBase> >::const_iterator it = reg_.begin();
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

int ReadPhraseUntilDividerOrEnd(const char* buf, const int sstart, const int end, vector<WordID>* p) {
  static const WordID kDIV = TD::Convert("|||");
  int ptr = sstart;
  while(ptr < end) {
    while(ptr < end && IsWhitespace(buf[ptr])) { ++ptr; }
    int start = ptr;
    while(ptr < end && !IsWhitespace(buf[ptr])) { ++ptr; }
    if (ptr == start) {cerr << "Warning! empty token.\n"; return ptr; }
    const WordID w = TD::Convert(string(buf, start, ptr - start));
    if (w == kDIV) return ptr;
    p->push_back(w);
  }
  assert(p->size() > 0);
  return ptr;  
}

void ParseLine(const char* buf, vector<WordID>* cur_key, ID2RuleStatistics* counts) {
  static const WordID kDIV = TD::Convert("|||");
  counts->clear();
  int ptr = 0;
  while(buf[ptr] != 0 && buf[ptr] != '\t') { ++ptr; }
  if (buf[ptr] != '\t') {
    cerr << "Missing tab separator between key and value!\n INPUT=" << buf << endl;
    exit(1);
  }
  cur_key->clear();
  // key is: "[X] ||| word word word"
  int tmpp = ReadPhraseUntilDividerOrEnd(buf, 0, ptr, cur_key);
  if (buf[tmpp] != '\t') {
    cur_key->push_back(kDIV);
    ReadPhraseUntilDividerOrEnd(buf, tmpp, ptr, cur_key);
  }
  ++ptr;
  int start = ptr;
  int end = ptr;
  int state = 0; // 0=reading label, 1=reading count
  vector<WordID> name;
  while(buf[ptr] != 0) {
    while(buf[ptr] != 0 && buf[ptr] != '|') { ++ptr; }
    if (buf[ptr] == '|') {
      ++ptr;
      if (buf[ptr] == '|') {
        ++ptr;
        if (buf[ptr] == '|') {
          ++ptr;
          end = ptr - 3;
          while (end > start && IsWhitespace(buf[end-1])) { --end; }
          if (start == end) {
            cerr << "Got empty token!\n  LINE=" << buf << endl;
            exit(1);
          }
          switch (state) {
            case 0: ++state; name.clear(); ReadPhraseUntilDividerOrEnd(buf, start, end, &name); break;
            case 1: --state; (*counts)[name].ParseRuleStatistics(buf, start, end); break;
            default: cerr << "Can't happen\n"; abort();
          }
          SkipWhitespace(buf, &ptr);
          start = ptr;
        }
      }
    }
  }
  end=ptr;
  while (end > start && IsWhitespace(buf[end-1])) { --end; }
  if (end > start) {
    switch (state) {
      case 0: ++state; name.clear(); ReadPhraseUntilDividerOrEnd(buf, start, end, &name); break;
      case 1: --state; (*counts)[name].ParseRuleStatistics(buf, start, end); break;
      default: cerr << "Can't happen\n"; abort();
    }
  }
}

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
    //result->set_value(fid_, log(info.counts.value(kCFE)));
    result->set_value(fid_, (info.counts.value(kCFE)));
    if (IsZero(info.counts.value(kCFE)))
      result->set_value(sfid_, 1);
  }
  const int fid_;
  const int sfid_;
  const int kCFE;
};

// The negative log of the condition rule probs 
// ignoring the identities of the  non-terminals. 
// i.e. the prob Hiero would assign.
struct XFeatures: public FeatureExtractor {
  XFeatures() :
    fid_fe(FD::Convert("XFE")),
    fid_ef(FD::Convert("XEF")),
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

  // compute statistics over keys, the same lhs-src-trg tuple may be seen
  // more than once
  virtual void ObserveUnfilteredRule(const WordID /*lhs*/,
                                     const vector<WordID>& src,
                                     const vector<WordID>& trg,
                                     const RuleStatistics& info) {
    RuleTuple r(-1, src, trg);
//    cerr << "   ObserveUnfilteredRule() in:" << r << " " << hash_value(r) << endl;
    map_rule(r);
    rule_counts.inc_if_exists(r, info.counts.value(kCFE));
    source_counts.inc_if_exists(r.source(), info.counts.value(kCFE));
    target_counts.inc_if_exists(r.target(), info.counts.value(kCFE));
//    cerr << "   ObserveUnfilteredRule() inc: " << r << " " << hash_value(r) << " " << info.counts.value(kCFE) << " to " << rule_counts(r) << endl;
  }

  virtual void ExtractFeatures(const WordID /*lhs*/,
                               const vector<WordID>& src,
                               const vector<WordID>& trg,
                               const RuleStatistics& /*info*/,
                               SparseVector<float>* result) const {
    RuleTuple r(-1, src, trg);
    map_rule(r);
    //result->set_value(fid_fe, log(target_counts(r.target())) - log(rule_counts(r)));
    //result->set_value(fid_ef, log(source_counts(r.source())) - log(rule_counts(r)));
    result->set_value(fid_ef, target_counts(r.target()));
    result->set_value(fid_fe, rule_counts(r));
    //result->set_value(fid_fe, (source_counts(r.source())));
  }

  void map_rule(RuleTuple& r) const {
    vector<WordID> indexes; int i=0;
    for (vector<WordID>::iterator it = r.target().begin(); it != r.target().end(); ++it) {
      if (validate_non_terminal(TD::Convert(*it)))
        indexes.push_back(*it);
    }
    for (vector<WordID>::iterator it = r.source().begin(); it != r.source().end(); ++it) {
      if (validate_non_terminal(TD::Convert(*it)))
        *it = indexes.at(i++);
    }
  }

  const int fid_fe, fid_ef;
  const int kCFE;
  RuleFreqCount rule_counts;
  FreqCount< vector<WordID> > source_counts, target_counts;
};

struct LabelledRuleConditionals: public FeatureExtractor {
  LabelledRuleConditionals() :
    fid_fe(FD::Convert("TLabelledFE")),
    fid_ef(FD::Convert("TLabelledEF")),
    kCFE(FD::Convert("CFE")) {}
  virtual void ObserveFilteredRule(const WordID /*lhs*/,
                                   const vector<WordID>& src,
                                   const vector<WordID>& trg) {
    RuleTuple r(-1, src, trg);
    rule_counts.inc(r, 0);
    cerr << "   ObservefilteredRule() inc: " << r << " " << hash_value(r) << endl;
//    map_rule(r);
    source_counts.inc(r.source(), 0);
    target_counts.inc(r.target(), 0);
  }

  // compute statistics over keys, the same lhs-src-trg tuple may be seen
  // more than once
  virtual void ObserveUnfilteredRule(const WordID /*lhs*/,
                                     const vector<WordID>& src,
                                     const vector<WordID>& trg,
                                     const RuleStatistics& info) {
    RuleTuple r(-1, src, trg);
    //cerr << "   ObserveUnfilteredRule() in:" << r << " " << hash_value(r) << endl;
    rule_counts.inc_if_exists(r, info.counts.value(kCFE));
    cerr << "   ObserveUnfilteredRule() inc_if_exists: " << r << " " << hash_value(r) << " " << info.counts.value(kCFE) << " to " << rule_counts(r) << endl;
//    map_rule(r);
    source_counts.inc_if_exists(r.source(), info.counts.value(kCFE));
    target_counts.inc_if_exists(r.target(), info.counts.value(kCFE));
  }

  virtual void ExtractFeatures(const WordID /*lhs*/,
                               const vector<WordID>& src,
                               const vector<WordID>& trg,
                               const RuleStatistics& info,
                               SparseVector<float>* result) const {
    RuleTuple r(-1, src, trg);
    //cerr << "   ExtractFeatures() in:" << " " << r.m_hash << endl;
    int r_freq = rule_counts(r);
    cerr << "   ExtractFeatures() count: " << r << " " << hash_value(r) << " " << info.counts.value(kCFE) << " | " << rule_counts(r) << endl;
    assert(r_freq == info.counts.value(kCFE));
    //cerr << "   ExtractFeatures() after:" << " " << r.hash << endl;
    //cerr << "   ExtractFeatures() in:" << r << " " << r_freq << " " << hash_value(r) << endl;
    //cerr << "   ExtractFeatures() in:" << r << " " << r_freq << endl;
//    map_rule(r);
    //result->set_value(fid_fe, log(target_counts(r.target())) - log(r_freq));
    //result->set_value(fid_ef, log(source_counts(r.source())) - log(r_freq));
    result->set_value(fid_ef, target_counts(r.target()));
    result->set_value(fid_fe, r_freq);
    //result->set_value(fid_fe, (source_counts(r.source())));
  }

  void map_rule(RuleTuple& r) const {
    vector<WordID> indexes; int i=0;
    for (vector<WordID>::iterator it = r.target().begin(); it != r.target().end(); ++it) {
      if (validate_non_terminal(TD::Convert(*it)))
        indexes.push_back(*it);
    }
    for (vector<WordID>::iterator it = r.source().begin(); it != r.source().end(); ++it) {
      if (validate_non_terminal(TD::Convert(*it)))
        *it = indexes.at(i++);
    }
  }

  const int fid_fe, fid_ef;
  const int kCFE;
  RuleFreqCount rule_counts;
  FreqCount< vector<WordID> > source_counts, target_counts;
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

int main(int argc, char** argv){
  FERegistry reg;
  reg.Register("LogRuleCount", new FEFactory<LogRuleCount>);
  reg.Register("LexProb", new FEFactory<LexProbExtractor>);
  reg.Register("XFeatures", new FEFactory<XFeatures>);
  reg.Register("LabelledRuleConditionals", new FEFactory<LabelledRuleConditionals>);
  po::variables_map conf;
  InitCommandLine(reg, argc, argv, &conf);
  aligned_corpus = conf["aligned_corpus"].as<string>();  // GLOBAL VAR
  ReadFile fg1(conf["filtered_grammar"].as<string>());

  vector<string> feats = conf["feature"].as<vector<string> >();
  vector<boost::shared_ptr<FeatureExtractor> > extractors(feats.size());
  for (int i = 0; i < feats.size(); ++i)
    extractors[i] = reg.Create(feats[i]);

  //score unscored grammar
  cerr << "Reading filtered grammar to detect keys..." << endl;
  char* buf = new char[MAX_LINE_LENGTH];

  ID2RuleStatistics acc, cur_counts;
  vector<WordID> key, cur_key,temp_key;
  WordID lhs = 0;
  vector<WordID> src;

  istream& fs1 = *fg1.stream();
  while(fs1) {
    fs1.getline(buf, MAX_LINE_LENGTH);
    if (buf[0] == 0) continue;
    ParseLine(buf, &cur_key, &cur_counts);
    src.resize(cur_key.size() - 2);
    for (int i = 0; i < src.size(); ++i) src.at(i) = cur_key.at(i+2);

    lhs = cur_key[0];
    for (ID2RuleStatistics::const_iterator it = cur_counts.begin(); it != cur_counts.end(); ++it) {
      for (int i = 0; i < extractors.size(); ++i)
        extractors[i]->ObserveFilteredRule(lhs, src, it->first);
    }
  }

  cerr << "Reading unfiltered grammar..." << endl;
  while(cin) {
    cin.getline(buf, MAX_LINE_LENGTH);
    if (buf[0] == 0) continue;
    ParseLine(buf, &cur_key, &cur_counts);
    src.resize(cur_key.size() - 2);
    for (int i = 0; i < src.size(); ++i) src[i] = cur_key[i+2];
    lhs = cur_key[0];
    for (ID2RuleStatistics::const_iterator it = cur_counts.begin(); it != cur_counts.end(); ++it) {
      for (int i = 0; i < extractors.size(); ++i)
        extractors[i]->ObserveUnfilteredRule(lhs, src, it->first, it->second);
    }
  }

  ReadFile fg2(conf["filtered_grammar"].as<string>());
  istream& fs2 = *fg2.stream();
  cerr << "Reading filtered grammar and adding features..." << endl;
  while(fs2) {
    fs2.getline(buf, MAX_LINE_LENGTH);
    if (buf[0] == 0) continue;
    ParseLine(buf, &cur_key, &cur_counts);
    src.resize(cur_key.size() - 2);
    for (int i = 0; i < src.size(); ++i) src[i] = cur_key[i+2];
    lhs = cur_key[0];

    //loop over all the Target side phrases that this source aligns to
    for (ID2RuleStatistics::const_iterator it = cur_counts.begin(); it != cur_counts.end(); ++it) {
      SparseVector<float> feats;
      for (int i = 0; i < extractors.size(); ++i)
        extractors[i]->ExtractFeatures(lhs, src, it->first, it->second, &feats);
      cout << TD::Convert(lhs) << " ||| " << TD::GetString(src) << " ||| " << TD::GetString(it->first) << " ||| ";
      feats.Write(false, &cout);
      cout << endl;
    }
  }
}

