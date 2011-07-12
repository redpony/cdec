#include <sstream>
#include <iostream>
#include <fstream>
#include <vector>

#include <boost/shared_ptr.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "sampler.h"
#include "filelib.h"
#include "stringlib.h"
#include "weights.h"
#include "scorer.h"
#include "inside_outside.h"
#include "hg_io.h"
#include "kbest.h"
#include "viterbi.h"

// This is Figure 4 (Algorithm Sampler) from Hopkins&May (2011)

using namespace std;
namespace po = boost::program_options;

boost::shared_ptr<MT19937> rng;

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("reference,r",po::value<vector<string> >(), "[REQD] Reference translation (tokenized text)")
        ("source,s",po::value<string>()->default_value(""), "Source file (ignored, except for AER)")
        ("loss_function,l",po::value<string>()->default_value("ibm_bleu"), "Loss function being optimized")
        ("input,i",po::value<string>()->default_value("-"), "Input file to map (- is STDIN)")
        ("weights,w",po::value<vector<string> >(), "[REQD] Weights files from previous and current iterations")
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
    cerr << "Please specify one or more weights using -w <WEIGHTS.TXT>\n";
    flag = true;
  }
  if (flag || conf->count("help")) {
    cerr << dcmdline_options << endl;
    exit(1);
  }
}

struct HypInfo {
  HypInfo(const vector<WordID>& h, const SparseVector<double>& feats) : hyp(h), g_(-100.0), x(feats) {}

  // lazy evaluation
  double g(const SentenceScorer& scorer) const {
    if (g_ == -100.0)
      g_ = scorer.ScoreCandidate(hyp)->ComputeScore();
    return g_;
  }
  vector<WordID> hyp;
  mutable double g_;
 public:
  SparseVector<double> x;
};

struct ThresholdAlpha {
  explicit ThresholdAlpha(double t = 0.05) : threshold(t) {}
  double operator()(double mag) const {
    if (mag < threshold) return 0.0; else return 1.0;
  }
  const double threshold;
};

struct TrainingInstance {
  TrainingInstance(const SparseVector<double>& feats, bool positive, double diff) : x(feats), y(positive), gdiff(diff) {}
  SparseVector<double> x;
#ifdef DEBUGGING_PRO
  vector<WordID> a;
  vector<WordID> b;
#endif
  bool y;
  double gdiff;
};

struct DiffOrder {
  bool operator()(const TrainingInstance& a, const TrainingInstance& b) const {
    return a.gdiff > b.gdiff;
  }
};

template<typename Alpha>
void Sample(const unsigned gamma, const unsigned xi, const vector<HypInfo>& J_i, const SentenceScorer& scorer, const Alpha& alpha_i, bool invert_score, vector<TrainingInstance>* pv) {
  vector<TrainingInstance> v;
  for (unsigned i = 0; i < gamma; ++i) {
    size_t a = rng->inclusive(0, J_i.size() - 1)();
    size_t b = rng->inclusive(0, J_i.size() - 1)();
    if (a == b) continue;
    double ga = J_i[a].g(scorer);
    double gb = J_i[b].g(scorer);
    bool positive = ga < gb;
    if (invert_score) positive = !positive;
    double gdiff = fabs(ga - gb);
    if (!gdiff) continue;
    if (rng->next() < alpha_i(gdiff)) {
      v.push_back(TrainingInstance((J_i[a].x - J_i[b].x).erase_zeros(), positive, gdiff));
#ifdef DEBUGGING_PRO
      v.back().a = J_i[a].hyp;
      v.back().b = J_i[b].hyp;
#endif
    }
  }
  vector<TrainingInstance>::iterator mid = v.begin() + xi;
  if (xi > v.size()) mid = v.end();
  partial_sort(v.begin(), mid, v.end(), DiffOrder());
  copy(v.begin(), mid, back_inserter(*pv));
#ifdef DEBUGGING_PRO
  if (v.size() >= 5)
    for (int i =0; i < 5; ++i) {
      cerr << v[i].gdiff << " y=" << v[i].y << "\tA:" << TD::GetString(v[i].a) << "\n\tB: " << TD::GetString(v[i].b) << endl;
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
  const string loss_function = conf["loss_function"].as<string>();
  ScoreType type = ScoreTypeFromString(loss_function);
  DocScorer ds(type, conf["reference"].as<vector<string> >(), conf["source"].as<string>());
  cerr << "Loaded " << ds.size() << " references for scoring with " << loss_function << endl;
  Hypergraph hg;
  string last_file;
  ReadFile in_read(conf["input"].as<string>());
  istream &in=*in_read.stream();
  const unsigned kbest_size = conf["kbest_size"].as<unsigned>();
  const unsigned gamma = conf["candidate_pairs"].as<unsigned>();
  const unsigned xi = conf["best_pairs"].as<unsigned>();
  vector<string> weights_files = conf["weights"].as<vector<string> >();
  vector<vector<double> > weights(weights_files.size());
  for (int i = 0; i < weights.size(); ++i) {
    Weights w;
    w.InitFromFile(weights_files[i]);
    w.InitVector(&weights[i]);
  }
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
    HypergraphIO::ReadFromJSON(rf.stream(), &hg);
    vector<HypInfo> J_i;
    int start = weights.size();
    start -= 4;
    if (start < 0) start = 0;
    for (int i = start; i < weights.size(); ++i) {
      hg.Reweight(weights[i]);
      KBest::KBestDerivations<vector<WordID>, ESentenceTraversal> kbest(hg, kbest_size);

      for (int i = 0; i < kbest_size; ++i) {
        const KBest::KBestDerivations<vector<WordID>, ESentenceTraversal>::Derivation* d =
          kbest.LazyKthBest(hg.nodes_.size() - 1, i);
        if (!d) break;
        J_i.push_back(HypInfo(d->yield, d->feature_values));
      }
    }

    Sample(gamma, xi, J_i, *ds[sent_id], ThresholdAlpha(0.05), (type == TER), &v);
    for (unsigned i = 0; i < v.size(); ++i) {
      const TrainingInstance& vi = v[i];
      cout << vi.y << "\t" << vi.x << endl;
      cout << (!vi.y) << "\t" << (vi.x * -1.0) << endl;
    }
  }
  return 0;
}

