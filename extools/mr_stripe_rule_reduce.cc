#include <iostream>
#include <vector>
#include <utility>
#include <cstdlib>
#include <tr1/unordered_map>

#include <boost/functional/hash.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "tdict.h"
#include "sentence_pair.h"
#include "fdict.h"
#include "extract.h"

using namespace std;
using namespace std::tr1;
namespace po = boost::program_options;

static const size_t MAX_LINE_LENGTH = 64000000;

bool use_hadoop_counters = false;

namespace {
  inline bool IsWhitespace(char c) { return c == ' ' || c == '\t'; }

  inline void SkipWhitespace(const char* buf, int* ptr) {
    while (buf[*ptr] && IsWhitespace(buf[*ptr])) { ++(*ptr); }
  }
}
void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("phrase_marginals,p", "Compute phrase marginals")
	("use_hadoop_counters,C", "Enable this if running inside Hadoop")
        ("bidir,b", "Rules are tagged as being F->E or E->F, invert E rules in output")
        ("help,h", "Print this help message and exit");
  po::options_description clo("Command line options");
  po::options_description dcmdline_options;
  dcmdline_options.add(opts);

  po::store(parse_command_line(argc, argv, dcmdline_options), *conf);
  po::notify(*conf);

  if (conf->count("help")) {
    cerr << "\nUsage: mr_stripe_rule_reduce [-options]\n";
    cerr << dcmdline_options << endl;
    exit(1);
  }
}

typedef unordered_map<vector<WordID>, RuleStatistics, boost::hash<vector<WordID> > > ID2RuleStatistics;

void PlusEquals(const ID2RuleStatistics& v, ID2RuleStatistics* self) {
  for (ID2RuleStatistics::const_iterator it = v.begin(); it != v.end(); ++it) {
    RuleStatistics& dest = (*self)[it->first];
    dest += it->second;
    // TODO - do something smarter about alignments?
    if (dest.aligns.empty() && !it->second.aligns.empty())
      dest.aligns = it->second.aligns;
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

void WriteKeyValue(const vector<WordID>& key, const ID2RuleStatistics& val) {
  cout << TD::GetString(key) << '\t';
  bool needdiv = false;
  for (ID2RuleStatistics::const_iterator it = val.begin(); it != val.end(); ++it) {
    if (needdiv) cout << " ||| "; else needdiv = true;
    cout << TD::GetString(it->first) << " ||| " << it->second;
  }
  cout << endl;
  if (use_hadoop_counters) cerr << "reporter:counter:UserCounters,RuleCount," << val.size() << endl;
}

void DoPhraseMarginals(const vector<WordID>& key, const bool bidir, ID2RuleStatistics* val) {
  static const WordID kF = TD::Convert("F");
  static const WordID kE = TD::Convert("E");
  static const int kCF = FD::Convert("CF");
  static const int kCE = FD::Convert("CE");
  static const int kCFE = FD::Convert("CFE");
  assert(key.size() > 0);
  int cur_marginal_id = kCF;
  if (bidir) {
    if (key[0] != kF && key[0] != kE) {
      cerr << "DoPhraseMarginals expects keys to have the from 'F|E [NT] word word word'\n";
      cerr << "  but got: " << TD::GetString(key) << endl;
      exit(1);
    }
    if (key[0] == kE) cur_marginal_id = kCE;
  }
  double tot = 0;
  for (ID2RuleStatistics::iterator it = val->begin(); it != val->end(); ++it)
    tot += it->second.counts.value(kCFE);
  for (ID2RuleStatistics::iterator it = val->begin(); it != val->end(); ++it) {
    it->second.counts.set_value(cur_marginal_id, tot);

    // prevent double counting of the joint
    if (cur_marginal_id == kCE) it->second.counts.clear_value(kCFE);
  }
}

void WriteWithInversions(const vector<WordID>& key, const ID2RuleStatistics& val) {
  static const WordID kE = TD::Convert("E");
  static const WordID kDIV = TD::Convert("|||");
  vector<WordID> new_key(key.size() - 1);
  for (int i = 1; i < key.size(); ++i)
    new_key[i - 1] = key[i];
  const bool do_invert = (key[0] == kE);
  if (!do_invert) {
    WriteKeyValue(new_key, val);
  } else {
    ID2RuleStatistics inv;
    assert(new_key.size() > 2);
    vector<WordID> tk(new_key.size() - 2);
    for (int i = 0; i < tk.size(); ++i)
      tk[i] = new_key[2 + i];
    RuleStatistics& inv_stats = inv[tk];
    for (ID2RuleStatistics::const_iterator it = val.begin(); it != val.end(); ++it) {
      inv_stats.counts = it->second.counts;
      vector<WordID> ekey(2 + it->first.size());
      ekey[0] = key[1];
      ekey[1] = kDIV;
      for (int i = 0; i < it->first.size(); ++i)
        ekey[2+i] = it->first[i];
      WriteKeyValue(ekey, inv);
    }
  }
}

int main(int argc, char** argv) {
  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);

  char* buf = new char[MAX_LINE_LENGTH];
  ID2RuleStatistics acc, cur_counts;
  vector<WordID> key, cur_key;
  int line = 0;
  use_hadoop_counters = conf.count("use_hadoop_counters") > 0;
  const bool phrase_marginals = conf.count("phrase_marginals") > 0;
  const bool bidir = conf.count("bidir") > 0;
  while(cin) {
    ++line;
    cin.getline(buf, MAX_LINE_LENGTH);
    if (buf[0] == 0) continue;
    ParseLine(buf, &cur_key, &cur_counts);
    if (cur_key != key) {
      if (key.size() > 0) {
        if (phrase_marginals)
          DoPhraseMarginals(key, bidir, &acc);
        if (bidir)
          WriteWithInversions(key, acc);
        else
          WriteKeyValue(key, acc);
        acc.clear();
      }
      key = cur_key;
    }
    PlusEquals(cur_counts, &acc);
  }
  if (key.size() > 0) {
    if (phrase_marginals)
      DoPhraseMarginals(key, bidir, &acc);
    if (bidir)
      WriteWithInversions(key, acc);
    else
      WriteKeyValue(key, acc);
  }
  return 0;
}

