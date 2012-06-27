#include <sstream>
#include <iostream>
#include <vector>
#include <limits>

#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "liblbfgs/lbfgs++.h"
#include "filelib.h"
#include "stringlib.h"
#include "weights.h"
#include "hg_io.h"
#include "kbest.h"
#include "viterbi.h"
#include "ns.h"
#include "ns_docscorer.h"
#include "candidate_set.h"
#include "risk.h"

using namespace std;
namespace po = boost::program_options;

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("reference,r",po::value<vector<string> >(), "[REQD] Reference translation (tokenized text)")
        ("weights,w",po::value<string>(), "[REQD] Weights files from current iterations")
        ("input,i",po::value<string>()->default_value("-"), "Input file to map (- is STDIN)")
        ("evaluation_metric,m",po::value<string>()->default_value("IBM_BLEU"), "Evaluation metric (ibm_bleu, koehn_bleu, nist_bleu, ter, meteor, etc.)")
        ("kbest_repository,R",po::value<string>(), "Accumulate k-best lists from previous iterations (parameter is path to repository)")
        ("kbest_size,k",po::value<unsigned>()->default_value(500u), "Top k-hypotheses to extract")
        ("help,h", "Help");
  po::options_description dcmdline_options;
  dcmdline_options.add(opts);
  po::store(parse_command_line(argc, argv, dcmdline_options), *conf);
  bool flag = false;
  if (!conf->count("reference")) {
    cerr << "Please specify one or more references using -r <REF.TXT>\n";
    flag = true;
  }
  if (!conf->count("weights")) {
    cerr << "Please specify weights using -w <WEIGHTS.TXT>\n";
    flag = true;
  }
  if (flag || conf->count("help")) {
    cerr << dcmdline_options << endl;
    exit(1);
  }
}

EvaluationMetric* metric = NULL;

struct RiskObjective {
  explicit RiskObjective(const vector<training::CandidateSet>& tr) : training(tr) {}
  double operator()(const vector<double>& x, double* g) const {
    fill(g, g + x.size(), 0.0);
    double obj = 0;
    for (unsigned i = 0; i < training.size(); ++i) {
      training::CandidateSetRisk risk(training[i], *metric);
      SparseVector<double> tg;
      double r = risk(x, &tg);
      obj += r;
      for (SparseVector<double>::iterator it = tg.begin(); it != tg.end(); ++it)
        g[it->first] += it->second;
    }
    cerr << (1-(obj / training.size())) << endl;
    return obj;
  }
  const vector<training::CandidateSet>& training;
};  

double LearnParameters(const vector<training::CandidateSet>& training,
                       const double C1,
                       const unsigned memory_buffers,
                       vector<weight_t>* px) {
  RiskObjective obj(training);
  LBFGS<RiskObjective> lbfgs(px, obj, memory_buffers, C1);
  lbfgs.MinimizeFunction();
  return 0;
}

// runs lines 4--15 of rampion algorithm
int main(int argc, char** argv) {
  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);
  const string evaluation_metric = conf["evaluation_metric"].as<string>();

  metric = EvaluationMetric::Instance(evaluation_metric);
  DocumentScorer ds(metric, conf["reference"].as<vector<string> >());
  cerr << "Loaded " << ds.size() << " references for scoring with " << evaluation_metric << endl;
  double goodsign = -1;
  double badsign = -goodsign;

  Hypergraph hg;
  string last_file;
  ReadFile in_read(conf["input"].as<string>());
  string kbest_repo;
  if (conf.count("kbest_repository")) {
    kbest_repo = conf["kbest_repository"].as<string>();
    MkDirP(kbest_repo);
  }
  istream &in=*in_read.stream();
  const unsigned kbest_size = conf["kbest_size"].as<unsigned>();
  vector<weight_t> weights;
  const string weightsf = conf["weights"].as<string>();
  Weights::InitFromFile(weightsf, &weights);
  double t = 0;
  for (unsigned i = 0; i < weights.size(); ++i)
    t += weights[i] * weights[i];
  if (t > 0) {
    for (unsigned i = 0; i < weights.size(); ++i)
      weights[i] /= sqrt(t);
  }
  string line, file;
  vector<training::CandidateSet> kis;
  cerr << "Loading hypergraphs...\n";
  while(getline(in, line)) {
    istringstream is(line);
    int sent_id;
    kis.resize(kis.size() + 1);
    training::CandidateSet& curkbest = kis.back();
    string kbest_file;
    if (kbest_repo.size()) {
      ostringstream os;
      os << kbest_repo << "/kbest." << sent_id << ".txt.gz";
      kbest_file = os.str();
      if (FileExists(kbest_file))
        curkbest.ReadFromFile(kbest_file);
    }
    is >> file >> sent_id;
    ReadFile rf(file);
    if (kis.size() % 5 == 0) { cerr << '.'; }
    if (kis.size() % 200 == 0) { cerr << " [" << kis.size() << "]\n"; }
    HypergraphIO::ReadFromJSON(rf.stream(), &hg);
    hg.Reweight(weights);
    curkbest.AddKBestCandidates(hg, kbest_size, ds[sent_id]);
    if (kbest_file.size())
      curkbest.WriteToFile(kbest_file);
  }
  cerr << "\nHypergraphs loaded.\n";
  weights.resize(FD::NumFeats());

  LearnParameters(kis, 0.0, 100, &weights);
  Weights::WriteToFile("-", weights);
  return 0;
}

