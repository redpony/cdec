#include <iostream>
#include <vector>

#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "filelib.h"
#include "tdict.h"
#include "scorer.h"

using namespace std;
namespace po = boost::program_options;

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("reference,r",po::value<vector<string> >(), "[REQD] Reference translation(s) (tokenized text file)")
        ("loss_function,l",po::value<string>()->default_value("ibm_bleu"), "Scoring metric (ibm_bleu, nist_bleu, koehn_bleu, ter, combi)")
        ("in_file,i", po::value<string>()->default_value("-"), "Input file")
        ("help,h", "Help");
  po::options_description dcmdline_options;
  dcmdline_options.add(opts);
  po::store(parse_command_line(argc, argv, dcmdline_options), *conf);
  bool flag = false;
  if (!conf->count("reference")) {
    cerr << "Please specify one or more references using -r <REF1.TXT> -r <REF2.TXT> ...\n";
    flag = true;
  }
  if (flag || conf->count("help")) {
    cerr << dcmdline_options << endl;
    exit(1);
  }
}

int main(int argc, char** argv) {
  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);
  const string loss_function = conf["loss_function"].as<string>();
  ScoreType type = ScoreTypeFromString(loss_function);
  DocScorer ds(type, conf["reference"].as<vector<string> >(), "");
  cerr << "Loaded " << ds.size() << " references for scoring with " << loss_function << endl;

  ReadFile rf(conf["in_file"].as<string>());
  ScoreP acc;
  istream& in = *rf.stream();
  int lc = 0;
  while(in) {
    string line;
    getline(in, line);
    if (line.empty() && !in) break;
    vector<WordID> sent;
    TD::ConvertSentence(line, &sent);
    ScoreP sentscore = ds[lc]->ScoreCandidate(sent);
    if (!acc) { acc = sentscore->GetZero(); }
    acc->PlusEquals(*sentscore);
    ++lc;
  }
  assert(lc > 0);
  if (lc > ds.size()) {
    cerr << "Too many (" << lc << ") translations in input, expected " << ds.size() << endl;
    return 1;
  }
  if (lc != ds.size())
    cerr << "Fewer sentences in hyp (" << lc << ") than refs ("
         << ds.size() << "): scoring partial set!\n";
  float score = acc->ComputeScore();
  string details;
  acc->ScoreDetails(&details);
  cerr << details << endl;
  cout << score << endl;
  return 0;
}
