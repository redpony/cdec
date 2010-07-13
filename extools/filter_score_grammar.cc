/*
 * Filter & score a grammar in striped format
 */
#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <utility>
#include <cstdlib>
#include <fstream>
#include <tr1/unordered_map>

#include "suffix_tree.h"
#include "sparse_vector.h"
#include "sentence_pair.h"
#include "extract.h"
#include "fdict.h"
#include "tdict.h"
#include "lex_trans_tbl.h"
#include "filelib.h"
#include "striped_grammar.h"

#include <boost/shared_ptr.hpp>
#include <boost/functional/hash.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

using namespace std;
using namespace std::tr1;
namespace po = boost::program_options;

static const size_t MAX_LINE_LENGTH = 64000000;

typedef unordered_map<vector<WordID>, RuleStatistics, boost::hash<vector<WordID> > > ID2RuleStatistics;

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("test_set,t", po::value<string>(), "Filter for this test set (not specified = no filtering)")
        ("top_e_given_f,n", po::value<size_t>()->default_value(30), "Keep top N rules, according to p(e|f). 0 for all")
        ("backoff_features", "Extract backoff X-features, assumes E, F, EF counts")
//        ("feature,f", po::value<vector<string> >()->composing(), "List of features to compute")
        ("aligned_corpus,c", po::value<string>(), "Aligned corpus (single line format)")
        ("help,h", "Print this help message and exit");
  po::options_description clo("Command line options");
  po::options_description dcmdline_options;
  dcmdline_options.add(opts);
  
  po::store(parse_command_line(argc, argv, dcmdline_options), *conf);
  po::notify(*conf);

  if (conf->count("help") || conf->count("aligned_corpus")==0) {
    cerr << "\nUsage: filter_score_grammar -t TEST-SET.fr -c ALIGNED_CORPUS.fr-en-al [-options] < grammar\n";
    cerr << dcmdline_options << endl;
    exit(1);
  }
}   
namespace {
  inline bool IsWhitespace(char c) { return c == ' ' || c == '\t'; }
  inline bool IsBracket(char c){return c == '[' || c == ']';}
  inline void SkipWhitespace(const char* buf, int* ptr) {
    while (buf[*ptr] && IsWhitespace(buf[*ptr])) { ++(*ptr); }
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

    if((IsBracket(buf[start]) and IsBracket(buf[ptr-1])) or( w == kDIV))
      p->push_back(1 * w);
    else {
      if (w == kDIV) return ptr;
      p->push_back(w);
    }
  }
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
  cur_key->push_back(kDIV);
  ReadPhraseUntilDividerOrEnd(buf, tmpp, ptr, cur_key);
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

struct SourceFilter {
  // return true to keep the rule, otherwise false
  virtual bool Matches(const vector<WordID>& key) const = 0;
  virtual ~SourceFilter() {}
};

struct DumbSuffixTreeFilter : SourceFilter {
  DumbSuffixTreeFilter(const string& corpus) :
      kDIV(TD::Convert("|||")) {
    cerr << "Build suffix tree from test set in " << corpus << endl;
    assert(FileExists(corpus));
    ReadFile rfts(corpus);
    istream& testSet = *rfts.stream();
    char* buf = new char[MAX_LINE_LENGTH];
    AnnotatedParallelSentence sent;

    /* process the data set to build suffix tree
     */
    while(!testSet.eof()) {
      testSet.getline(buf, MAX_LINE_LENGTH);
      if (buf[0] == 0) continue;

      //hack to read in the test set using AnnotatedParallelSentence
      strcat(buf," ||| fake ||| 0-0");
      sent.ParseInputLine(buf);

      //add each successive suffix to the tree
      for(int i=0; i<sent.f_len; i++)
        root.InsertPath(sent.f, i, sent.f_len - 1);
    }
    delete[] buf;
  }
  virtual bool Matches(const vector<WordID>& key) const {
    const Node<int>* curnode = &root;
    const int ks = key.size() - 1;
    for(int i=0; i < ks; i++) {
      const string& word = TD::Convert(key[i]);
      if (key[i] == kDIV || (word[0] == '[' && word[word.size() - 1] == ']')) { // non-terminal
        curnode = &root;
      } else if (curnode) {
        curnode = curnode->Extend(key[i]);
        if (!curnode) return false;
      }
    }
    return true;
  }
  const WordID kDIV;
  Node<int> root;
};

struct FeatureExtractor {
  FeatureExtractor(const std::string& name) : extractor_name(name) {}
  virtual void ExtractFeatures(const vector<WordID>& lhs_src,
                               const vector<WordID>& trg,
                               const RuleStatistics& info,
                               SparseVector<float>* result) const = 0;
  virtual ~FeatureExtractor() {}
  const string extractor_name;
};

static bool IsZero(float f) { return (f > 0.999 && f < 1.001); }

struct LogRuleCount : public FeatureExtractor {
  LogRuleCount() :
    FeatureExtractor("LogRuleCount"),
    fid_(FD::Convert("LogRuleCount")),
    sfid_(FD::Convert("SingletonRule")),
    kCFE(FD::Convert("CFE")) {}
  virtual void ExtractFeatures(const vector<WordID>& lhs_src,
                               const vector<WordID>& trg,
                               const RuleStatistics& info,
                               SparseVector<float>* result) const {
    (void) lhs_src; (void) trg;
    result->set_value(fid_, log(info.counts.value(kCFE)));
    if (IsZero(info.counts.value(kCFE)))
      result->set_value(sfid_, 1);
  }
  const int fid_;
  const int sfid_;
  const int kCFE;
};

struct LogECount : public FeatureExtractor {
  LogECount() :
    FeatureExtractor("LogECount"),
    sfid_(FD::Convert("SingletonE")),
    fid_(FD::Convert("LogECount")), kCE(FD::Convert("CE")) {}
  virtual void ExtractFeatures(const vector<WordID>& lhs_src,
                               const vector<WordID>& trg,
                               const RuleStatistics& info,
                               SparseVector<float>* result) const {
    (void) lhs_src; (void) trg;
    assert(info.counts.value(kCE) > 0);
    result->set_value(fid_, log(info.counts.value(kCE)));
    if (IsZero(info.counts.value(kCE)))
      result->set_value(sfid_, 1);
  }
  const int sfid_;
  const int fid_;
  const int kCE;
};

struct LogFCount : public FeatureExtractor {
  LogFCount() :
    FeatureExtractor("LogFCount"),
    sfid_(FD::Convert("SingletonF")),
    fid_(FD::Convert("LogFCount")), kCF(FD::Convert("CF")) {}
  virtual void ExtractFeatures(const vector<WordID>& lhs_src,
                               const vector<WordID>& trg,
                               const RuleStatistics& info,
                               SparseVector<float>* result) const {
    (void) lhs_src; (void) trg;
    assert(info.counts.value(kCF) > 0);
    result->set_value(fid_, log(info.counts.value(kCF)));
    if (IsZero(info.counts.value(kCF)))
      result->set_value(sfid_, 1);
  }
  const int sfid_;
  const int fid_;
  const int kCF;
};

struct EGivenFExtractor : public FeatureExtractor {
  EGivenFExtractor() :
    FeatureExtractor("EGivenF"),
    fid_(FD::Convert("EGivenF")), kCF(FD::Convert("CF")), kCFE(FD::Convert("CFE")) {}
  virtual void ExtractFeatures(const vector<WordID>& lhs_src,
                               const vector<WordID>& trg,
                               const RuleStatistics& info,
                               SparseVector<float>* result) const {
    (void) lhs_src; (void) trg;
    assert(info.counts.value(kCF) > 0.0f);
    result->set_value(fid_, safenlog(info.counts.value(kCFE) / info.counts.value(kCF)));
  }
  const int fid_, kCF, kCFE;
};

struct FGivenEExtractor : public FeatureExtractor {
  FGivenEExtractor() :
    FeatureExtractor("FGivenE"),
    fid_(FD::Convert("FGivenE")), kCE(FD::Convert("CE")), kCFE(FD::Convert("CFE")) {}
  virtual void ExtractFeatures(const vector<WordID>& lhs_src,
                               const vector<WordID>& trg,
                               const RuleStatistics& info,
                               SparseVector<float>* result) const {
    (void) lhs_src; (void) trg;
    assert(info.counts.value(kCE) > 0.0f);
    result->set_value(fid_, safenlog(info.counts.value(kCFE) / info.counts.value(kCE)));
  }
  const int fid_, kCE, kCFE;
};

// this extracts the lexical translation prob features
// in BOTH directions.
struct LexProbExtractor : public FeatureExtractor {
  LexProbExtractor(const std::string& corpus) :
      FeatureExtractor("LexProb"), e2f_(FD::Convert("LexE2F")), f2e_(FD::Convert("LexF2E")) {
    ReadFile rf(corpus);
    //create lexical translation table
    cerr << "Computing lexical translation probabilities from " << corpus << "..." << endl;
    char* buf = new char[MAX_LINE_LENGTH];
    istream& alignment = *rf.stream();
    while(alignment) {
      alignment.getline(buf, MAX_LINE_LENGTH);
      if (buf[0] == 0) continue;
      table.createTTable(buf);              
    }
    delete[] buf;
#if 0
    bool PRINT_TABLE=false;
    if (PRINT_TABLE) {
      ofstream trans_table;
      trans_table.open("lex_trans_table.out");
      for(map < pair<WordID,WordID>,int >::iterator it = table.word_translation.begin(); it != table.word_translation.end(); ++it) {
        trans_table <<  TD::Convert(trg.first) <<  "|||" << TD::Convert(trg.second) << "==" << it->second << "//" << table.total_foreign[trg.first] << "//" << table.total_english[trg.second] << endl;
      } 
      trans_table.close();
    }
#endif
  }

  virtual void ExtractFeatures(const vector<WordID>& lhs_src,
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
              cerr <<  TD::Convert(lhs_src[ita->first + 2]) << "-" << TD::Convert(trg[ita->second]);
            }

            //Lookup this alignment probability in the table
            int temp = table.word_translation[pair<WordID,WordID> (lhs_src[ita->first+2],trg[ita->second])];
            float f2e=0, e2f=0;
            if ( table.total_foreign[lhs_src[ita->first+2]] != 0)
              f2e = (float) temp / table.total_foreign[lhs_src[ita->first+2]];
            if ( table.total_english[trg[ita->second]] !=0 )
              e2f = (float) temp / table.total_english[trg[ita->second]];
            if (DEBUG) printf (" %d %E %E\n", temp, f2e, e2f);
              
            //local counts to keep track of which things haven't been aligned, to later compute their null alignment
            if (foreign_aligned.count(lhs_src[ita->first+2])) {
              foreign_aligned[ lhs_src[ita->first+2] ].first++;
              foreign_aligned[ lhs_src[ita->first+2] ].second += e2f;
            } else {
              foreign_aligned[ lhs_src[ita->first+2] ] = pair<int,float> (1,e2f);
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
           for(int i=0;i<lhs_src.size(); i++) {
               if (!table.total_foreign.count(lhs_src[i])) continue;      //if we dont have it in the translation table, we won't know its lexical weight
               
               if (foreign_aligned.count(lhs_src[i])) 
                 {
                   pair<int, float> temp_lex_prob = foreign_aligned[lhs_src[i]];
                   final_lex_e2f *= temp_lex_prob.second / temp_lex_prob.first;
                 }
               else //dealing with null alignment
                 {
                   int temp_count = table.word_translation[pair<WordID,WordID> (lhs_src[i],NULL_)];
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
  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);
  const int max_options = conf["top_e_given_f"].as<size_t>();;
  ifstream alignment (conf["aligned_corpus"].as<string>().c_str());
  istream& unscored_grammar = cin;
  ostream& scored_grammar = cout;

  boost::shared_ptr<SourceFilter> filter;
  if (conf.count("test_set"))
    filter.reset(new DumbSuffixTreeFilter(conf["test_set"].as<string>()));

  // TODO make this list configurable
  vector<boost::shared_ptr<FeatureExtractor> > extractors;
  if (conf.count("backoff_features")) {
    extractors.push_back(boost::shared_ptr<FeatureExtractor>(new LogRuleCount));
    extractors.push_back(boost::shared_ptr<FeatureExtractor>(new LogECount));
    extractors.push_back(boost::shared_ptr<FeatureExtractor>(new LogFCount));
    extractors.push_back(boost::shared_ptr<FeatureExtractor>(new EGivenFExtractor));
    extractors.push_back(boost::shared_ptr<FeatureExtractor>(new FGivenEExtractor));
    extractors.push_back(boost::shared_ptr<FeatureExtractor>(new LexProbExtractor(conf["aligned_corpus"].as<string>())));
  } else {
    extractors.push_back(boost::shared_ptr<FeatureExtractor>(new LogRuleCount));
    extractors.push_back(boost::shared_ptr<FeatureExtractor>(new LogFCount));
    extractors.push_back(boost::shared_ptr<FeatureExtractor>(new LexProbExtractor(conf["aligned_corpus"].as<string>())));
  }

  //score unscored grammar
  cerr <<"Scoring grammar..." << endl;
  char* buf = new char[MAX_LINE_LENGTH];

  ID2RuleStatistics acc, cur_counts;
  vector<WordID> key, cur_key,temp_key;
  int line = 0;

  const int kLogRuleCount = FD::Convert("LogRuleCount");
  multimap<float, string> options; 
  while(!unscored_grammar.eof())
    {
      ++line;
      options.clear();
      unscored_grammar.getline(buf, MAX_LINE_LENGTH);
      if (buf[0] == 0) continue;
      ParseLine(buf, &cur_key, &cur_counts);
      if (!filter || filter->Matches(cur_key)) {
        //loop over all the Target side phrases that this source aligns to
        for (ID2RuleStatistics::const_iterator it = cur_counts.begin(); it != cur_counts.end(); ++it) {

          SparseVector<float> feats;
          for (int i = 0; i < extractors.size(); ++i)
            extractors[i]->ExtractFeatures(cur_key, it->first, it->second, &feats);

           ostringstream os;
           os << TD::GetString(cur_key)
              << ' ' << TD::GetString(it->first) << " ||| ";
           feats.Write(false, &os);
           options.insert(make_pair(-feats.value(kLogRuleCount), os.str()));
        }
        int ocount = 0;
        for (multimap<float,string>::iterator it = options.begin(); it != options.end(); ++it) {
          scored_grammar << it->second << endl;
          ++ocount;
          if (ocount == max_options) break;
        }
      }
    }
}

