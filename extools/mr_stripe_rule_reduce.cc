#include <iostream>
#include <vector>
#include <utility>
#include <cstdlib>
#include <tr1/unordered_map>

#include <boost/functional/hash.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "striped_grammar.h"
#include "tdict.h"
#include "sentence_pair.h"
#include "fdict.h"
#include "extract.h"

using namespace std;
using namespace std::tr1;
namespace po = boost::program_options;

static const size_t MAX_LINE_LENGTH = 64000000;

bool use_hadoop_counters = false;

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

void PlusEquals(const ID2RuleStatistics& v, ID2RuleStatistics* self) {
  for (ID2RuleStatistics::const_iterator it = v.begin(); it != v.end(); ++it) {
    RuleStatistics& dest = (*self)[it->first];
    dest += it->second;
    // TODO - do something smarter about alignments?
    if (dest.aligns.empty() && !it->second.aligns.empty())
      dest.aligns = it->second.aligns;
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
    if (cur_marginal_id == kCE) it->second.counts.erase(kCFE);
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

struct Reducer {
  Reducer(bool phrase_marginals, bool bidir) : pm_(phrase_marginals), bidir_(bidir) {}

  void ProcessLine(const vector<WordID>& key, const ID2RuleStatistics& rules) {
    if (cur_key_ != key) {
      if (cur_key_.size() > 0) Emit();
      acc_.clear();
      cur_key_ = key;
    }
    PlusEquals(rules, &acc_);
  }

  ~Reducer() {
    Emit();
  }

  void Emit() {
    if (pm_)
      DoPhraseMarginals(cur_key_, bidir_, &acc_);
    if (bidir_)
      WriteWithInversions(cur_key_, acc_);
    else
      WriteKeyValue(cur_key_, acc_);
  }

  const bool pm_;
  const bool bidir_;
  vector<WordID> cur_key_;
  ID2RuleStatistics acc_;
};

void cb(const vector<WordID>& key, const ID2RuleStatistics& contexts, void* red) {
  static_cast<Reducer*>(red)->ProcessLine(key, contexts);
}


int main(int argc, char** argv) {
  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);

  char* buf = new char[MAX_LINE_LENGTH];
  vector<WordID> key, cur_key;
  int line = 0;
  use_hadoop_counters = conf.count("use_hadoop_counters") > 0;
  const bool phrase_marginals = conf.count("phrase_marginals") > 0;
  const bool bidir = conf.count("bidir") > 0;
  Reducer reducer(phrase_marginals, bidir);
  StripedGrammarLexer::ReadContexts(&cin, cb, &reducer);
  return 0;
}

