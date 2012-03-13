#include <sstream>
#include <iostream>
#include <fstream>
#include <vector>
#include <tr1/unordered_map>

#include <boost/functional/hash.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "sampler.h"
#include "filelib.h"
#include "stringlib.h"
#include "weights.h"
#include "inside_outside.h"
#include "hg_io.h"
#include "kbest.h"
#include "viterbi.h"
#include "ns.h"
#include "ns_docscorer.h"

// This is Figure 4 (Algorithm Sampler) from Hopkins&May (2011)

using namespace std;
namespace po = boost::program_options;

struct ApproxVectorHasher {
  static const size_t MASK = 0xFFFFFFFFull;
  union UType {
    double f;   // leave as double
    size_t i;
  };
  static inline double round(const double x) {
    UType t;
    t.f = x;
    size_t r = t.i & MASK;
    if ((r << 1) > MASK)
      t.i += MASK - r + 1;
    else
      t.i &= (1ull - MASK);
    return t.f;
  }
  size_t operator()(const SparseVector<weight_t>& x) const {
    size_t h = 0x573915839;
    for (SparseVector<weight_t>::const_iterator it = x.begin(); it != x.end(); ++it) {
      UType t;
      t.f = it->second;
      if (t.f) {
        size_t z = (t.i >> 32);
        boost::hash_combine(h, it->first);
        boost::hash_combine(h, z);
      }
    }
    return h;
  }
};

struct ApproxVectorEquals {
  bool operator()(const SparseVector<weight_t>& a, const SparseVector<weight_t>& b) const {
    SparseVector<weight_t>::const_iterator bit = b.begin();
    for (SparseVector<weight_t>::const_iterator ait = a.begin(); ait != a.end(); ++ait) {
      if (bit == b.end() ||
          ait->first != bit->first ||
          ApproxVectorHasher::round(ait->second) != ApproxVectorHasher::round(bit->second))
        return false;
      ++bit;
    }
    if (bit != b.end()) return false;
    return true;
  }
};

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

struct HypInfo {
  HypInfo() : g_(-100.0f) {}
  HypInfo(const vector<WordID>& h, const SparseVector<weight_t>& feats) : hyp(h), g_(-100.0f), x(feats) {}

  // lazy evaluation
  double g(const SegmentEvaluator& scorer, const EvaluationMetric* metric) const {
    if (g_ == -100.0f) {
      SufficientStats ss;
      scorer.Evaluate(hyp, &ss);
      g_ = metric->ComputeScore(ss);
    }
    return g_;
  }
  vector<WordID> hyp;
  mutable float g_;
  SparseVector<weight_t> x;
};

struct HypInfoCompare {
  bool operator()(const HypInfo& a, const HypInfo& b) const {
    ApproxVectorEquals comp;
    return (a.hyp == b.hyp && comp(a.x,b.x));
  }
};

struct HypInfoHasher {
  size_t operator()(const HypInfo& x) const {
    boost::hash<vector<WordID> > hhasher;
    ApproxVectorHasher vhasher;
    size_t ha = hhasher(x.hyp);
    boost::hash_combine(ha, vhasher(x.x));
    return ha;
  }
};

void WriteKBest(const string& file, const vector<HypInfo>& kbest) {
  WriteFile wf(file);
  ostream& out = *wf.stream();
  out.precision(10);
  for (int i = 0; i < kbest.size(); ++i) {
    out << TD::GetString(kbest[i].hyp) << endl;
    out << kbest[i].x << endl;
  }
}

void ParseSparseVector(string& line, size_t cur, SparseVector<weight_t>* out) {
  SparseVector<weight_t>& x = *out;
  size_t last_start = cur;
  size_t last_comma = string::npos;
  while(cur <= line.size()) {
    if (line[cur] == ' ' || cur == line.size()) {
      if (!(cur > last_start && last_comma != string::npos && cur > last_comma)) {
        cerr << "[ERROR] " << line << endl << "  position = " << cur << endl;
        exit(1);
      }
      const int fid = FD::Convert(line.substr(last_start, last_comma - last_start));
      if (cur < line.size()) line[cur] = 0;
      const double val = strtod(&line[last_comma + 1], NULL);
      x.set_value(fid, val);

      last_comma = string::npos;
      last_start = cur+1;
    } else {
      if (line[cur] == '=')
        last_comma = cur;
    }
    ++cur;
  }
}

void ReadKBest(const string& file, vector<HypInfo>* kbest) {
  cerr << "Reading from " << file << endl;
  ReadFile rf(file);
  istream& in = *rf.stream();
  string cand;
  string feats;
  while(getline(in, cand)) {
    getline(in, feats);
    assert(in);
    kbest->push_back(HypInfo());
    TD::ConvertSentence(cand, &kbest->back().hyp);
    ParseSparseVector(feats, 0, &kbest->back().x);
  }
  cerr << "  read " << kbest->size() << " hypotheses\n";
}

void Dedup(vector<HypInfo>* h) {
  cerr << "Dedup in=" << h->size();
  tr1::unordered_set<HypInfo, HypInfoHasher, HypInfoCompare> u;
  while(h->size() > 0) {
    u.insert(h->back());
    h->pop_back();
  }
  tr1::unordered_set<HypInfo, HypInfoHasher, HypInfoCompare>::iterator it = u.begin();
  while (it != u.end()) {
    h->push_back(*it);
    it = u.erase(it);
  }
  cerr << "  out=" << h->size() << endl;
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
            const vector<HypInfo>& J_i,
            const SegmentEvaluator& scorer,
            const EvaluationMetric* metric,
            vector<TrainingInstance>* pv) {
  const bool invert_score = metric->IsErrorMetric();
  vector<TrainingInstance> v1, v2;
  float avg_diff = 0;
  for (unsigned i = 0; i < gamma; ++i) {
    const size_t a = rng->inclusive(0, J_i.size() - 1)();
    const size_t b = rng->inclusive(0, J_i.size() - 1)();
    if (a == b) continue;
    float ga = J_i[a].g(scorer, metric);
    float gb = J_i[b].g(scorer, metric);
    bool positive = gb < ga;
    if (invert_score) positive = !positive;
    const float gdiff = fabs(ga - gb);
    if (!gdiff) continue;
    avg_diff += gdiff;
    SparseVector<weight_t> xdiff = (J_i[a].x - J_i[b].x).erase_zeros();
    if (xdiff.empty()) {
      cerr << "Empty diff:\n  " << TD::GetString(J_i[a].hyp) << endl << "x=" << J_i[a].x << endl;
      cerr << "  " << TD::GetString(J_i[b].hyp) << endl << "x=" << J_i[b].x << endl;
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
    vector<HypInfo> J_i;
    os << kbest_repo << "/kbest." << sent_id << ".txt.gz";
    const string kbest_file = os.str();
    if (FileExists(kbest_file))
      ReadKBest(kbest_file, &J_i);
    HypergraphIO::ReadFromJSON(rf.stream(), &hg);
    hg.Reweight(weights);
    KBest::KBestDerivations<vector<WordID>, ESentenceTraversal> kbest(hg, kbest_size);

    for (int i = 0; i < kbest_size; ++i) {
      const KBest::KBestDerivations<vector<WordID>, ESentenceTraversal>::Derivation* d =
        kbest.LazyKthBest(hg.nodes_.size() - 1, i);
      if (!d) break;
      J_i.push_back(HypInfo(d->yield, d->feature_values));
    }
    Dedup(&J_i);
    WriteKBest(kbest_file, J_i);

    Sample(gamma, xi, J_i, *ds[sent_id], metric, &v);
    for (unsigned i = 0; i < v.size(); ++i) {
      const TrainingInstance& vi = v[i];
      cout << vi.y << "\t" << vi.x << endl;
      cout << (!vi.y) << "\t" << (vi.x * -1.0) << endl;
    }
  }
  return 0;
}

