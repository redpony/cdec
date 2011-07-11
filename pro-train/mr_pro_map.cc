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
        ("source,s",po::value<string>(), "Source file (ignored, except for AER)")
        ("loss_function,l",po::value<string>()->default_value("ibm_bleu"), "Loss function being optimized")
        ("input,i",po::value<string>()->default_value("-"), "Input file to map (- is STDIN)")
        ("weights,w",po::value<string>(), "[REQD] Current weights file")
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
  if (flag || conf->count("help")) {
    cerr << dcmdline_options << endl;
    exit(1);
  }
}

struct HypInfo {
  HypInfo(const vector<WordID>& h, const SparseVector<double>& feats) : hyp(h), g_(-1), x(feats) {}
  double g() {
    return g_;
  }
 private:
  int sent_id;
  vector<WordID> hyp;
  double g_;
 public:
  SparseVector<double> x;
};

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
  while(in) {
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
    KBest::KBestDerivations<vector<WordID>, ESentenceTraversal> kbest(hg, kbest_size);

    vector<HypInfo> J_i;
    for (int i = 0; i < kbest_size; ++i) {
      const KBest::KBestDerivations<vector<WordID>, ESentenceTraversal>::Derivation* d =
        kbest.LazyKthBest(hg.nodes_.size() - 1, i);
      if (!d) break;
      float sentscore = ds[sent_id]->ScoreCandidate(d->yield)->ComputeScore();
      // if (invert_score) sentscore *= -1.0;
      // cerr << TD::GetString(d->yield) << " ||| " << d->score << " ||| " << sentscore << endl;
      d->feature_values;
      sentscore;
    }
  }
  return 0;
}

