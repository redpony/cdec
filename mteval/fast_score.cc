#include <iostream>
#include <vector>

#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "stringlib.h"
#include "filelib.h"
#include "tdict.h"
#include "ns.h"
#include "ns_docscorer.h"

using namespace std;
namespace po = boost::program_options;

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("reference,r",po::value<vector<string> >(), "[1 or more required] Reference translation(s) in tokenized text files")
        ("evaluation_metric,m",po::value<string>()->default_value("IBM_BLEU"), "Evaluation metric (ibm_bleu, koehn_bleu, nist_bleu, ter, meteor, etc.)")
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
  string loss_function = UppercaseString(conf["evaluation_metric"].as<string>());
  if (loss_function == "COMBI") {
    cerr << "WARNING: 'combi' metric is no longer supported, switching to 'COMB:TER=-0.5;IBM_BLEU=0.5'\n";
    loss_function = "COMB:TER=-0.5;IBM_BLEU=0.5";
  } else if (loss_function == "BLEU") {
    cerr << "WARNING: 'BLEU' is ambiguous, assuming 'IBM_BLEU'\n";
    loss_function = "IBM_BLEU";
  }
  EvaluationMetric* metric = EvaluationMetric::Instance(loss_function);
  DocumentScorer ds(metric, conf["reference"].as<vector<string> >());
  cerr << "Loaded " << ds.size() << " references for scoring with " << loss_function << endl;

  ReadFile rf(conf["in_file"].as<string>());
  SufficientStats acc;
  istream& in = *rf.stream();
  int lc = 0;
  string line;
  while(getline(in, line)) {
    vector<WordID> sent;
    TD::ConvertSentence(line, &sent);
    SufficientStats t;
    ds[lc]->Evaluate(sent, &t);
    acc += t;
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
  float score = metric->ComputeScore(acc);
  const string details = metric->DetailedScore(acc);
  cerr << details << endl;
  cout << score << endl;
  return 0;
}
