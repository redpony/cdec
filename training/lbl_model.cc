#include <iostream>

#include "config.h"
#ifndef HAVE_EIGEN
  int main() { std::cerr << "Please rebuild with --with-eigen PATH\n"; return 1; }
#else

#include <cstdlib>
#include <algorithm>
#include <cmath>
#include <set>
#include <cstring> // memset
#include <ctime>

#include <boost/math/special_functions/fpclassify.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>
#include <Eigen/Dense>

#include "optimize.h"
#include "array2d.h"
#include "m.h"
#include "lattice.h"
#include "stringlib.h"
#include "filelib.h"
#include "tdict.h"

namespace po = boost::program_options;
using namespace std;

#define kDIMENSIONS 110
typedef Eigen::Matrix<float, kDIMENSIONS, 1> RVector;
typedef Eigen::Matrix<float, 1, kDIMENSIONS> RTVector;
typedef Eigen::Matrix<float, kDIMENSIONS, kDIMENSIONS> TMatrix;
vector<RVector> r_src, r_trg;

bool InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("input,i",po::value<string>(),"Input file")
        ("iterations,I",po::value<unsigned>()->default_value(1000),"Number of iterations of training")
        ("regularization_strength,C",po::value<float>()->default_value(0.1),"L2 regularization strength (0 for no regularization)")
        ("eta,e", po::value<float>()->default_value(0.1f), "Eta for SGD")
        ("random_seed,s", po::value<unsigned>(), "Random seed")
        ("diagonal_tension,T", po::value<double>()->default_value(4.0), "How sharp or flat around the diagonal is the alignment distribution (0 = uniform, >0 sharpens)")
        ("testset,x", po::value<string>(), "After training completes, compute the log likelihood of this set of sentence pairs under the learned model");
  po::options_description clo("Command line options");
  clo.add_options()
        ("config", po::value<string>(), "Configuration file")
        ("help,h", "Print this help message and exit");
  po::options_description dconfig_options, dcmdline_options;
  dconfig_options.add(opts);
  dcmdline_options.add(opts).add(clo);
  
  po::store(parse_command_line(argc, argv, dcmdline_options), *conf);
  if (conf->count("config")) {
    ifstream config((*conf)["config"].as<string>().c_str());
    po::store(po::parse_config_file(config, dconfig_options), *conf);
  }
  po::notify(*conf);

  if (argc < 2 || conf->count("help")) {
    cerr << "Usage " << argv[0] << " [OPTIONS] -i corpus.fr-en\n";
    cerr << dcmdline_options << endl;
    return false;
  }
  return true;
}

void Normalize(RVector* v) {
  float norm = v->norm();
  assert(norm > 0.0f);
  *v /= norm;
}

void Flatten(const TMatrix& m, vector<double>* v) {
  unsigned c = 0;
  v->resize(kDIMENSIONS * kDIMENSIONS);
  for (unsigned i = 0; i < kDIMENSIONS; ++i)
    for (unsigned j = 0; j < kDIMENSIONS; ++j) {
      assert(boost::math::isnormal(m(i, j)));
      (*v)[c++] = m(i,j);
    }
}

void Unflatten(const vector<double>& v, TMatrix* m) {
  unsigned c = 0;
  for (unsigned i = 0; i < kDIMENSIONS; ++i)
    for (unsigned j = 0; j < kDIMENSIONS; ++j) {
      assert(boost::math::isnormal(v[c]));
      (*m)(i, j) = v[c++];
    }
}

double ApplyRegularization(const double C,
                           const vector<double>& weights,
                           vector<double>* g) {
  assert(weights.size() == g->size());
  double reg = 0;
  for (size_t i = 0; i < weights.size(); ++i) {
    const double& w_i = weights[i];
    double& g_i = (*g)[i];
    reg += C * w_i * w_i;
    g_i += 2 * C * w_i;
  }
  return reg;
}

int main(int argc, char** argv) {
  po::variables_map conf;
  if (!InitCommandLine(argc, argv, &conf)) return 1;
  const string fname = conf["input"].as<string>();
  const float reg_strength = conf["regularization_strength"].as<float>();
  const bool has_l2 = reg_strength;
  assert(reg_strength >= 0.0f);
  const int ITERATIONS = conf["iterations"].as<unsigned>();
  const float eta = conf["eta"].as<float>();
  const double diagonal_tension = conf["diagonal_tension"].as<double>();
  bool SGD = false;
  if (diagonal_tension < 0.0) {
    cerr << "Invalid value for diagonal_tension: must be >= 0\n";
    return 1;
  }
  string testset;
  if (conf.count("testset")) testset = conf["testset"].as<string>();

  unsigned lc = 0;
  vector<double> unnormed_a_i;
  string line;
  string ssrc, strg;
  bool flag = false;
  Lattice src, trg;
  vector<WordID> vocab_e;
  { // read through corpus, initialize int map, check lines are good
    set<WordID> svocab_e;
    cerr << "INITIAL READ OF " << fname << endl;
    ReadFile rf(fname);
    istream& in = *rf.stream();
    while(getline(in, line)) {
      ++lc;
      if (lc % 1000 == 0) { cerr << '.'; flag = true; }
      if (lc %50000 == 0) { cerr << " [" << lc << "]\n" << flush; flag = false; }
      ParseTranslatorInput(line, &ssrc, &strg);
      LatticeTools::ConvertTextToLattice(ssrc, &src);
      LatticeTools::ConvertTextToLattice(strg, &trg);
      if (src.size() == 0 || trg.size() == 0) {
        cerr << "Error: " << lc << "\n" << line << endl;
        assert(src.size() > 0);
        assert(trg.size() > 0);
      }
      if (src.size() > unnormed_a_i.size())
        unnormed_a_i.resize(src.size());
      for (unsigned i = 0; i < trg.size(); ++i) {
        assert(trg[i].size() == 1);
        svocab_e.insert(trg[i][0].label);
      }
    }
    copy(svocab_e.begin(), svocab_e.end(), back_inserter(vocab_e));
  }
  if (flag) cerr << endl;
  cerr << "Number of target word types: " << vocab_e.size() << endl;
  const float num_examples = lc;

  LBFGSOptimizer lbfgs(kDIMENSIONS * kDIMENSIONS, 100);
  r_trg.resize(TD::NumWords() + 1);
  r_src.resize(TD::NumWords() + 1);
  if (conf.count("random_seed")) {
    srand(conf["random_seed"].as<unsigned>());
  } else {
    unsigned seed = time(NULL);
    cerr << "Random seed: " << seed << endl;
    srand(seed);
  }
  TMatrix t = TMatrix::Random() / 50.0;
  for (unsigned i = 1; i < r_trg.size(); ++i) {
    r_trg[i] = RVector::Random();
    r_src[i] = RVector::Random();
    r_trg[i][i % kDIMENSIONS] = 0.5;
    r_src[i][(i-1) % kDIMENSIONS] = 0.5;
    Normalize(&r_trg[i]);
    Normalize(&r_src[i]);
  }
  vector<set<unsigned> > trg_pos(TD::NumWords() + 1);

  // do optimization
  TMatrix g = TMatrix::Zero();
  vector<TMatrix> exp_src;
  vector<double> z_src;
  vector<double> flat_g, flat_t;
  Flatten(t, &flat_t);
  for (int iter = 0; iter < ITERATIONS; ++iter) {
    cerr << "ITERATION " << (iter + 1) << endl;
    ReadFile rf(fname);
    istream& in = *rf.stream();
    double likelihood = 0;
    double denom = 0.0;
    lc = 0;
    flag = false;
    g *= 0;
    while(getline(in, line)) {
      ++lc;
      if (lc % 1000 == 0) { cerr << '.'; flag = true; }
      if (lc %50000 == 0) { cerr << " [" << lc << "]\n" << flush; flag = false; }
      ParseTranslatorInput(line, &ssrc, &strg);
      LatticeTools::ConvertTextToLattice(ssrc, &src);
      LatticeTools::ConvertTextToLattice(strg, &trg);
      denom += trg.size();

      exp_src.clear(); exp_src.resize(src.size(), TMatrix::Zero());
      z_src.clear(); z_src.resize(src.size(), 0.0);
      Array2D<TMatrix> exp_refs(src.size(), trg.size(), TMatrix::Zero());
      Array2D<double> z_refs(src.size(), trg.size(), 0.0);
      for (unsigned j = 0; j < trg.size(); ++j)
        trg_pos[trg[j][0].label].insert(j);

      for (unsigned i = 0; i < src.size(); ++i) {
        const RVector& r_s = r_src[src[i][0].label];
        const RTVector pred = r_s.transpose() * t;
        TMatrix& exp_m = exp_src[i];
        double& z = z_src[i];
        for (unsigned k = 0; k < vocab_e.size(); ++k) {
          const WordID v_k = vocab_e[k];
          const RVector& r_t = r_trg[v_k];
          const double dot_prod = pred * r_t;
          const double u = exp(dot_prod);
          z += u;
          const TMatrix v = r_s * r_t.transpose() * u;
          exp_m += v;
          set<unsigned>& ref_locs = trg_pos[v_k];
          if (!ref_locs.empty()) {
            for (set<unsigned>::iterator it = ref_locs.begin(); it != ref_locs.end(); ++it) {
              TMatrix& exp_ref_ij = exp_refs(i, *it);
              double& z_ref_ij = z_refs(i, *it);
              z_ref_ij += u;
              exp_ref_ij += v;
            }
          }
        }
      }
      for (unsigned j = 0; j < trg.size(); ++j)
        trg_pos[trg[j][0].label].clear();

      // model expectations for a single target generation with
      // uniform alignment prior
      double m_z = 0;
      TMatrix m_exp = TMatrix::Zero();
      for (unsigned i = 0; i < src.size(); ++i) {
        m_exp += exp_src[i];
        m_z += z_src[i];
      }
      m_exp /= m_z;

      Array2D<bool> al(src.size(), trg.size(), false);
      for (unsigned j = 0; j < trg.size(); ++j) {
        double ref_z = 0;
        TMatrix ref_exp = TMatrix::Zero();
        int max_i = 0;
        double max_s = -9999999;
        for (unsigned i = 0; i < src.size(); ++i) {
          ref_exp += exp_refs(i, j);
          ref_z += z_refs(i, j);
          if (log(z_refs(i, j)) > max_s) {
            max_s = log(z_refs(i, j));
            max_i = i;
          }
          // TODO handle alignment prob
        }
        if (ref_z <= 0) { 
          cerr << "TRG=" << TD::Convert(trg[j][0].label) << endl;
          cerr << " LINE=" << line << endl;
          cerr << " REF_EXP=\n" << ref_exp << endl;
          cerr << " M_EXP=\n" << m_exp << endl;
          abort();
        }
        al(max_i, j) = true;
        ref_exp /= ref_z;
        g += m_exp - ref_exp;
        likelihood += log(ref_z) - log(m_z);
        if (SGD) {
          t -= g * eta / num_examples;
          g *= 0;
        }
      }
      
      if (iter == (ITERATIONS - 1) || lc == 28) { cerr << al << endl; }
    }
    if (flag) { cerr << endl; }

    const double base2_likelihood = likelihood / log(2);
    cerr << "  log_e likelihood: " << likelihood << endl;
    cerr << "  log_2 likelihood: " << base2_likelihood << endl;
    cerr << "     cross entropy: " << (-base2_likelihood / denom) << endl;
    cerr << "        perplexity: " << pow(2.0, -base2_likelihood / denom) << endl;
    if (!SGD) {
      Flatten(g, &flat_g);
      double obj = -likelihood;
      if (has_l2) {
        const double r = ApplyRegularization(reg_strength,
                                             flat_t,
                                             &flat_g);
        obj += r;
        cerr << "    regularization: " << r << endl;
      }
      lbfgs.Optimize(obj, flat_g, &flat_t);
      Unflatten(flat_t, &t);
      if (lbfgs.HasConverged()) break;
    }
    cerr << t << endl;
  }
  cerr << "TRANSLATION MATRIX:" << endl << t << endl;
  return 0;
}

#endif

