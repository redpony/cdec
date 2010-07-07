/*
 * Filter a grammar in striped format
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
        ("test_set,t", po::value<string>(), "Filter for this test set")
        ("top_e_given_f,n", po::value<size_t>()->default_value(30), "Keep top N rules, according to p(e|f). 0 for all")
        ("help,h", "Print this help message and exit");
  po::options_description clo("Command line options");
  po::options_description dcmdline_options;
  dcmdline_options.add(opts);
  
  po::store(parse_command_line(argc, argv, dcmdline_options), *conf);
  po::notify(*conf);

  if (conf->count("help") || conf->count("test_set")==0) {
    cerr << "\nUsage: filter_grammar -t TEST-SET.fr [-options] < grammar\n";
    cerr << dcmdline_options << endl;
    exit(1);
  }
}   
namespace {
  inline bool IsWhitespace(char c) { return c == ' ' || c == '\t'; }
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

int main(int argc, char** argv){
  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);
  const int max_options = conf["top_e_given_f"].as<size_t>();;
  istream& unscored_grammar = cin;

  cerr << "Loading test set " << conf["test_set"].as<string>() << "...\n";
  boost::shared_ptr<SourceFilter> filter;
  filter.reset(new DumbSuffixTreeFilter(conf["test_set"].as<string>()));

  cerr << "Filtering...\n";
  //score unscored grammar
  char* buf = new char[MAX_LINE_LENGTH];

  ID2RuleStatistics acc, cur_counts;
  vector<WordID> key, cur_key,temp_key;
  int line = 0;

  multimap<float, ID2RuleStatistics::const_iterator> options; 
  const int kCOUNT = FD::Convert("CFE");
  while(!unscored_grammar.eof())
    {
      ++line;
      options.clear();
      unscored_grammar.getline(buf, MAX_LINE_LENGTH);
      if (buf[0] == 0) continue;
      ParseLine(buf, &cur_key, &cur_counts);
      if (!filter || filter->Matches(cur_key)) {
        // sort by counts
        for (ID2RuleStatistics::const_iterator it = cur_counts.begin(); it != cur_counts.end(); ++it) {
          options.insert(make_pair(-it->second.counts.value(kCOUNT), it));
        }
        int ocount = 0;
        cout << TD::GetString(cur_key) << '\t';

        bool first = true;
        for (multimap<float,ID2RuleStatistics::const_iterator>::iterator it = options.begin(); it != options.end(); ++it) {
          if (first) { first = false; } else { cout << " ||| "; }
          cout << TD::GetString(it->second->first) << " ||| " << it->second->second;
          ++ocount;
          if (ocount == max_options) break;
        }
        cout << endl;
      }
    }
}

