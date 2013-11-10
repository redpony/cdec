#include <sstream>
#include <iostream>
#include <fstream>
#include <vector>

#include <boost/functional/hash.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "candidate_set.h"
#include "sampler.h"
#include "filelib.h"
#include "stringlib.h"
#include "weights.h"
#include "inside_outside.h"
#include "hg_io.h"
#include "ns.h"
#include "ns_docscorer.h"

// This is Figure 4 (Algorithm Sampler) from Hopkins&May (2011)

using namespace std;
namespace po = boost::program_options;

boost::shared_ptr<MT19937> rng;

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("reference,r",po::value<vector<string> >(), "[REQD] Reference translation (tokenized text)")
        ("weights,w",po::value<string>(), "[REQD] Weights files from current iterations")
        ("kbest_repository,K",po::value<string>()->default_value("./kbest"),"K-best list repository (directory)")
        ("input,i",po::value<string>()->default_value("-"), "Input file to map (- is STDIN)")
        ("source,s",po::value<string>()->default_value(""), "Source file (ignored, except for AER)")
        ("evaluation_metric,m",po::value<string>()->default_value("IBM_BLEU"), "Evaluation metric (ibm_bleu, koehn_bleu, nist_bleu, ter, meteor, etc.)")
        ("kbest_size,k",po::value<unsigned>()->default_value(1500u), "Top k-hypotheses to extract")
        ("candidate_pairs,G", po::value<unsigned>()->default_value(5000u), "Number of pairs to sample per hypothesis (Gamma)")
        ("best_pairs,X", po::value<unsigned>()->default_value(50u), "Number of pairs, ranked by magnitude of objective delta, to retain (Xi)")
        ("random_seed,S", po::value<uint32_t>(), "Random seed (if not specified, /dev/random will be used)")
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

struct ThresholdAlpha {
  explicit ThresholdAlpha(double t = 0.05) : threshold(t) {}
  double operator()(double mag) const {
    if (mag < threshold) return 0.0; else return 1.0;
  }
  const double threshold;
};

struct TrainingInstance {
  TrainingInstance(const SparseVector<weight_t>& feats, bool positive, float diff) : x(feats), y(positive), gdiff(diff) {}
  SparseVector<weight_t> x;
#undef DEBUGGING_PRO
#ifdef DEBUGGING_PRO
  vector<WordID> a;
  vector<WordID> b;
#endif
  bool y;
  float gdiff;
};
#ifdef DEBUGGING_PRO
ostream& operator<<(ostream& os, const TrainingInstance& d) {
  return os << d.gdiff << " y=" << d.y << "\tA:" << TD::GetString(d.a) << "\n\tB: " << TD::GetString(d.b) << "\n\tX: " << d.x;
}
#endif

struct DiffOrder {
  bool operator()(const TrainingInstance& a, const TrainingInstance& b) const {
    return a.gdiff > b.gdiff;
  }
};

void Sample(const unsigned gamma,
            const unsigned xi,
            const training::CandidateSet& J_i,
            const EvaluationMetric* metric,
            vector<TrainingInstance>* pv) {
  const bool invert_score = metric->IsErrorMetric();
  vector<TrainingInstance> v1, v2;
  float avg_diff = 0;
  for (unsigned i = 0; i < gamma; ++i) {
    const size_t a = rng->inclusive(0, J_i.size() - 1)();
    const size_t b = rng->inclusive(0, J_i.size() - 1)();
    if (a == b) continue;
    float ga = metric->ComputeScore(J_i[a].eval_feats);
    float gb = metric->ComputeScore(J_i[b].eval_feats);
    bool positive = gb < ga;
    if (invert_score) positive = !positive;
    const float gdiff = fabs(ga - gb);
    if (!gdiff) continue;
    avg_diff += gdiff;
    SparseVector<weight_t> xdiff = (J_i[a].fmap - J_i[b].fmap).erase_zeros();
    if (xdiff.empty()) {
      cerr << "Empty diff:\n  " << TD::GetString(J_i[a].ewords) << endl << "x=" << J_i[a].fmap << endl;
      cerr << "  " << TD::GetString(J_i[b].ewords) << endl << "x=" << J_i[b].fmap << endl;
      continue;
    }
    v1.push_back(TrainingInstance(xdiff, positive, gdiff));
#ifdef DEBUGGING_PRO
    v1.back().a = J_i[a].hyp;
    v1.back().b = J_i[b].hyp;
    cerr << "N: " << v1.back() << endl;
#endif
  }
  avg_diff /= v1.size();

  for (unsigned i = 0; i < v1.size(); ++i) {
    double p = 1.0 / (1.0 + exp(-avg_diff - v1[i].gdiff));
    // cerr << "avg_diff=" << avg_diff << "  gdiff=" << v1[i].gdiff << "  p=" << p << endl;
    if (rng->next() < p) v2.push_back(v1[i]);
  }
  vector<TrainingInstance>::iterator mid = v2.begin() + xi;
  if (xi > v2.size()) mid = v2.end();
  partial_sort(v2.begin(), mid, v2.end(), DiffOrder());
  copy(v2.begin(), mid, back_inserter(*pv));
#ifdef DEBUGGING_PRO
  if (v2.size() >= 5) {
    for (int i =0; i < (mid - v2.begin()); ++i) {
      cerr << v2[i] << endl;
    }
    cerr << pv->back() << endl;
  }
#endif
}

int main(int argc, char** argv) {
  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);
  if (conf.count("random_seed"))
    rng.reset(new MT19937(conf["random_seed"].as<uint32_t>()));
  else
    rng.reset(new MT19937);
  const string evaluation_metric = conf["evaluation_metric"].as<string>();

  EvaluationMetric* metric = EvaluationMetric::Instance(evaluation_metric);
  DocumentScorer ds(metric, conf["reference"].as<vector<string> >());
  cerr << "Loaded " << ds.size() << " references for scoring with " << evaluation_metric << endl;

  Hypergraph hg;
  string last_file;
  ReadFile in_read(conf["input"].as<string>());
  istream &in=*in_read.stream();
  const unsigned kbest_size = conf["kbest_size"].as<unsigned>();
  const unsigned gamma = conf["candidate_pairs"].as<unsigned>();
  const unsigned xi = conf["best_pairs"].as<unsigned>();
  string weightsf = conf["weights"].as<string>();
  vector<weight_t> weights;
  Weights::InitFromFile(weightsf, &weights);
  string kbest_repo = conf["kbest_repository"].as<string>();
  MkDirP(kbest_repo);
  while(in) {
    vector<TrainingInstance> v;
    string line;
    getline(in, line);
    if (line.empty()) continue;
    istringstream is(line);
    int sent_id;
    string file;
    // path-to-file (JSON) sent_id
    is >> file >> sent_id;
    ReadFile rf(file);
    ostringstream os;
    training::CandidateSet J_i;
    os << kbest_repo << "/kbest." << sent_id << ".txt.gz";
    const string kbest_file = os.str();
    if (FileExists(kbest_file))
      J_i.ReadFromFile(kbest_file);
    HypergraphIO::ReadFromJSON(rf.stream(), &hg);
    hg.Reweight(weights);
    J_i.AddKBestCandidates(hg, kbest_size, ds[sent_id]);
    J_i.WriteToFile(kbest_file);

    Sample(gamma, xi, J_i, metric, &v);
    for (unsigned i = 0; i < v.size(); ++i) {
      const TrainingInstance& vi = v[i];
      cout << vi.y << "\t" << vi.x << endl;
      cout << (!vi.y) << "\t" << (vi.x * -1.0) << endl;
    }
  }
  return 0;
}

