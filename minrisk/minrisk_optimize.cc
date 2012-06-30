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
#include "entropy.h"

using namespace std;
namespace po = boost::program_options;

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("reference,r",po::value<vector<string> >(), "[REQD] Reference translation (tokenized text)")
        ("weights,w",po::value<string>(), "[REQD] Weights files from current iterations")
        ("input,i",po::value<string>()->default_value("-"), "Input file to map (- is STDIN)")
        ("evaluation_metric,m",po::value<string>()->default_value("IBM_BLEU"), "Evaluation metric (ibm_bleu, koehn_bleu, nist_bleu, ter, meteor, etc.)")
        ("temperature,T",po::value<double>()->default_value(0.0), "Temperature parameter for objective (>0 increases the entropy)")
        ("l1_strength,C",po::value<double>()->default_value(0.0), "L1 regularization strength")
        ("memory_buffers,M",po::value<unsigned>()->default_value(20), "Memory buffers used in LBFGS")
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
  explicit RiskObjective(const vector<training::CandidateSet>& tr, const double temp) : training(tr), T(temp) {}
  double operator()(const vector<double>& x, double* g) const {
    fill(g, g + x.size(), 0.0);
    double obj = 0;
    double h = 0;
    for (unsigned i = 0; i < training.size(); ++i) {
      training::CandidateSetRisk risk(training[i], *metric);
      training::CandidateSetEntropy entropy(training[i]);
      SparseVector<double> tg, hg;
      double r = risk(x, &tg);
      double hh = entropy(x, &hg);
      h += hh;
      obj += r;
      for (SparseVector<double>::iterator it = tg.begin(); it != tg.end(); ++it)
        g[it->first] += it->second;
      if (T) {
        for (SparseVector<double>::iterator it = hg.begin(); it != hg.end(); ++it)
          g[it->first] += T * it->second;
      }
    }
    cerr << (1-(obj / training.size())) << "  H=" << h << endl;
    return obj - T * h;
  }
  const vector<training::CandidateSet>& training;
  const double T; // temperature for entropy regularization
};  

double LearnParameters(const vector<training::CandidateSet>& training,
                       const double temp, // > 0 increases the entropy, < 0 decreases the entropy
                       const double C1,
                       const unsigned memory_buffers,
                       vector<weight_t>* px) {
  RiskObjective obj(training, temp);
  LBFGS<RiskObjective> lbfgs(px, obj, memory_buffers, C1);
  lbfgs.MinimizeFunction();
  return 0;
}

#if 0
struct FooLoss {
  double operator()(const vector<double>& x, double* g) const {
    fill(g, g + x.size(), 0.0);
    training::CandidateSet cs;
    training::CandidateSetEntropy cse(cs);
    cs.cs.resize(3);
    cs.cs[0].fmap.set_value(FD::Convert("F1"), -1.0);
    cs.cs[1].fmap.set_value(FD::Convert("F2"), 1.0);
    cs.cs[2].fmap.set_value(FD::Convert("F1"), 2.0);
    cs.cs[2].fmap.set_value(FD::Convert("F2"), 0.5);
    SparseVector<double> xx;
    double h = cse(x, &xx);
    cerr << cse(x, &xx) << endl; cerr << "G: " << xx << endl;
    for (SparseVector<double>::iterator i = xx.begin(); i != xx.end(); ++i)
      g[i->first] += i->second;
    return -h;
  }
};
#endif

int main(int argc, char** argv) {
#if 0
  training::CandidateSet cs;
  training::CandidateSetEntropy cse(cs);
  cs.cs.resize(3);
  cs.cs[0].fmap.set_value(FD::Convert("F1"), -1.0);
  cs.cs[1].fmap.set_value(FD::Convert("F2"), 1.0);
  cs.cs[2].fmap.set_value(FD::Convert("F1"), 2.0);
  cs.cs[2].fmap.set_value(FD::Convert("F2"), 0.5);
  FooLoss foo;
  vector<double> ww(FD::NumFeats()); ww[FD::Convert("F1")] = 1.0;
  LBFGS<FooLoss> lbfgs(&ww, foo, 100, 0.0);
  lbfgs.MinimizeFunction();
  return 1;
#endif
  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);
  const string evaluation_metric = conf["evaluation_metric"].as<string>();

  metric = EvaluationMetric::Instance(evaluation_metric);
  DocumentScorer ds(metric, conf["reference"].as<vector<string> >());
  cerr << "Loaded " << ds.size() << " references for scoring with " << evaluation_metric << endl;

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

  double c1 = conf["l1_strength"].as<double>();
  double temp = conf["temperature"].as<double>();
  unsigned m = conf["memory_buffers"].as<unsigned>();
  LearnParameters(kis, temp, c1, m, &weights);
  Weights::WriteToFile("-", weights);
  return 0;
}

