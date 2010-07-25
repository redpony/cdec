/*
 * Filter a grammar in striped format
 */
#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <utility>
#include <tr1/unordered_map>

#include "suffix_tree.h"
#include "sparse_vector.h"
#include "sentence_pair.h"
#include "extract.h"
#include "fdict.h"
#include "tdict.h"
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

struct SourceFilter {
  // return true to keep the rule, otherwise false
  virtual bool Matches(const vector<WordID>& key) const = 0;
  virtual ~SourceFilter() {}
};

struct DumbSuffixTreeFilter : SourceFilter {
  DumbSuffixTreeFilter(const string& corpus) {
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
  virtual bool Matches(const vector<WordID>& src_rhs) const {
    const Node<int>* curnode = &root;
    for(int i=0; i < src_rhs.size(); i++) {
      if (src_rhs[i] <= 0) {
        curnode = &root;
      } else if (curnode) {
        curnode = curnode->Extend(src_rhs[i]);
        if (!curnode) return false;
      }
    }
    return true;
  }
  Node<int> root;
};

boost::shared_ptr<SourceFilter> filter;
multimap<float, ID2RuleStatistics::const_iterator> options;
int kCOUNT;
int max_options;

void cb(WordID lhs, const vector<WordID>& src_rhs, const ID2RuleStatistics& rules, void*) {
  options.clear();
  if (!filter || filter->Matches(src_rhs)) {
    for (ID2RuleStatistics::const_iterator it = rules.begin(); it != rules.end(); ++it) {
      options.insert(make_pair(-it->second.counts.get(kCOUNT), it));
    }
    int ocount = 0;
    cout << '[' << TD::Convert(-lhs) << ']' << " ||| ";
    WriteNamed(src_rhs, &cout);
    cout << '\t';
    bool first = true;
    for (multimap<float,ID2RuleStatistics::const_iterator>::iterator it = options.begin(); it != options.end(); ++it) {
      if (first) { first = false; } else { cout << " ||| "; }
      WriteAnonymous(it->second->first, &cout);
      cout << " ||| " << it->second->second;
      ++ocount;
      if (ocount == max_options) break;
    }
    cout << endl;
  }
}

int main(int argc, char** argv){
  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);
  max_options = conf["top_e_given_f"].as<size_t>();;
  kCOUNT = FD::Convert("CFE");
  istream& unscored_grammar = cin;
  cerr << "Loading test set " << conf["test_set"].as<string>() << "...\n";
  filter.reset(new DumbSuffixTreeFilter(conf["test_set"].as<string>()));
  cerr << "Filtering...\n";
  StripedGrammarLexer::ReadStripedGrammar(&unscored_grammar, cb, NULL);
}

