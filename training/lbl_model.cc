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

#ifdef HAVE_MPI
#include <boost/mpi/timer.hpp>
#include <boost/mpi.hpp>
#include <boost/archive/text_oarchive.hpp>
namespace mpi = boost::mpi;
#endif
#include <boost/math/special_functions/fpclassify.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>
#include <Eigen/Dense>

#include "corpus_tools.h"
#include "optimize.h"
#include "array2d.h"
#include "m.h"
#include "lattice.h"
#include "stringlib.h"
#include "filelib.h"
#include "tdict.h"

namespace po = boost::program_options;
using namespace std;

#define kDIMENSIONS 10
typedef Eigen::Matrix<double, kDIMENSIONS, 1> RVector;
typedef Eigen::Matrix<double, 1, kDIMENSIONS> RTVector;
typedef Eigen::Matrix<double, kDIMENSIONS, kDIMENSIONS> TMatrix;
vector<RVector> r_src, r_trg;

#if HAVE_MPI
namespace boost {
namespace serialization {

template<class Archive>
void serialize(Archive & ar, RVector & v, const unsigned int version) {
  for (unsigned i = 0; i < kDIMENSIONS; ++i)
    ar & v[i];
}

} // namespace serialization
} // namespace boost
#endif

bool InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("input,i",po::value<string>(),"Input file")
        ("iterations,I",po::value<unsigned>()->default_value(1000),"Number of iterations of training")
        ("regularization_strength,C",po::value<double>()->default_value(0.1),"L2 regularization strength (0 for no regularization)")
        ("eta", po::value<double>()->default_value(0.1f), "Eta for SGD")
        ("source_embeddings,f", po::value<string>(), "File containing source embeddings (if unset, random vectors will be used)")
        ("target_embeddings,e", po::value<string>(), "File containing target embeddings (if unset, random vectors will be used)")
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
  double norm = v->norm();
  assert(norm > 0.0f);
  *v /= norm;
}

void Flatten(const TMatrix& m, vector<double>* v) {
  unsigned c = 0;
  v->resize(kDIMENSIONS * kDIMENSIONS);
  for (unsigned i = 0; i < kDIMENSIONS; ++i)
    for (unsigned j = 0; j < kDIMENSIONS; ++j) {
      assert(boost::math::isfinite(m(i, j)));
      (*v)[c++] = m(i,j);
    }
}

void Unflatten(const vector<double>& v, TMatrix* m) {
  unsigned c = 0;
  for (unsigned i = 0; i < kDIMENSIONS; ++i)
    for (unsigned j = 0; j < kDIMENSIONS; ++j) {
      assert(boost::math::isfinite(v[c]));
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

void LoadEmbeddings(const string& filename, vector<RVector>* pv) {
  vector<RVector>& v = *pv;
  cerr << "Reading embeddings from " << filename << " ...\n";
  ReadFile rf(filename);
  istream& in = *rf.stream();
  string line;
  unsigned lc = 0;
  while(getline(in, line)) {
    ++lc;
    size_t cur = line.find(' ');
    if (cur == string::npos || cur == 0) {
      cerr << "Parse error reading line " << lc << ":\n" << line << endl;
      abort();
    }
    WordID w = TD::Convert(line.substr(0, cur));
    if (w >= v.size()) continue;
    RVector& curv = v[w];
    line[cur] = 0;
    size_t start = cur + 1;
    cur = start + 1;
    size_t c = 0;
    while(cur < line.size()) {
      if (line[cur] == ' ') {
        line[cur] = 0;
        curv[c++] = strtod(&line[start], NULL);
        start = cur + 1;
        cur = start;
        if (c == kDIMENSIONS) break;
      }
      ++cur;
    }
    if (c < kDIMENSIONS && cur != start) {
      if (cur < line.size()) line[cur] = 0;
      curv[c++] = strtod(&line[start], NULL);
    }
    if (c != kDIMENSIONS) {
      static bool first = true;
      if (first) {
        cerr << " read " << c << " dimensions from embedding file, but built with " << kDIMENSIONS << " (filling in with random values)\n";
        first = false;
      }
      for (; c < kDIMENSIONS; ++c) curv[c] = rand();
    }
    if (c == kDIMENSIONS && cur != line.size()) {
      static bool first = true;
      if (first) {
        cerr << " embedding file contains more dimensions than configured with, truncating.\n";
        first = false;
      }
    }
  }
}

int main(int argc, char** argv) {
#ifdef HAVE_MPI
  std::cerr << "**MPI enabled.\n";
  mpi::environment env(argc, argv);
  mpi::communicator world;
  const int size = world.size(); 
  const int rank = world.rank();
#else
  std::cerr << "**MPI disabled.\n";
  const int rank = 0;
  const int size = 1;
#endif
  po::variables_map conf;
  if (!InitCommandLine(argc, argv, &conf)) return 1;
  const string fname = conf["input"].as<string>();
  const double reg_strength = conf["regularization_strength"].as<double>();
  const bool has_l2 = reg_strength;
  assert(reg_strength >= 0.0f);
  const int ITERATIONS = conf["iterations"].as<unsigned>();
  const double eta = conf["eta"].as<double>();
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
  bool flag = false;
  vector<vector<WordID> > srcs, trgs;
  vector<WordID> vocab_e;
  {
    set<WordID> svocab_e, svocab_f;
    CorpusTools::ReadFromFile(fname, &srcs, NULL, &trgs, &svocab_e, rank, size);
    copy(svocab_e.begin(), svocab_e.end(), back_inserter(vocab_e));
  }
  cerr << "Number of target word types: " << vocab_e.size() << endl;
  const double num_examples = lc;

  boost::shared_ptr<LBFGSOptimizer> lbfgs;
  if (rank == 0)
    lbfgs.reset(new LBFGSOptimizer(kDIMENSIONS * kDIMENSIONS, 100));
  r_trg.resize(TD::NumWords() + 1);
  r_src.resize(TD::NumWords() + 1);
  vector<set<unsigned> > trg_pos(TD::NumWords() + 1);

  if (conf.count("random_seed")) {
    srand(conf["random_seed"].as<unsigned>());
  } else {
    unsigned seed = time(NULL) + rank * 100;
    cerr << "Random seed: " << seed << endl;
    srand(seed);
  }
  
  TMatrix t = TMatrix::Zero();
  if (rank == 0) {
    t = TMatrix::Random() / 50.0;
    for (unsigned i = 1; i < r_trg.size(); ++i) {
      r_trg[i] = RVector::Random();
      r_src[i] = RVector::Random();
    }
    if (conf.count("source_embeddings"))
      LoadEmbeddings(conf["source_embeddings"].as<string>(), &r_src);
    if (conf.count("target_embeddings"))
      LoadEmbeddings(conf["target_embeddings"].as<string>(), &r_trg);
  }

  // do optimization
  TMatrix g = TMatrix::Zero();
  vector<TMatrix> exp_src;
  vector<double> z_src;
  vector<double> flat_g, flat_t, rcv_grad;
  Flatten(t, &flat_t);
  bool converged = false;
#if HAVE_MPI
  mpi::broadcast(world, &flat_t[0], flat_t.size(), 0);
  mpi::broadcast(world, r_trg, 0);
  mpi::broadcast(world, r_src, 0);
#endif
  cerr << "rank=" << rank << ": " << r_trg[0][4] << endl;
  for (int iter = 0; !converged && iter < ITERATIONS; ++iter) {
    if (rank == 0) cerr << "ITERATION " << (iter + 1) << endl;
    Unflatten(flat_t, &t);
    double likelihood = 0;
    double denom = 0.0;
    lc = 0;
    flag = false;
    g *= 0;
    for (unsigned i = 0; i < srcs.size(); ++i) {
      const vector<WordID>& src = srcs[i];
      const vector<WordID>& trg = trgs[i];
      ++lc;
      if (rank == 0 && lc % 1000 == 0) { cerr << '.'; flag = true; }
      if (rank == 0 && lc %50000 == 0) { cerr << " [" << lc << "]\n" << flush; flag = false; }
      denom += trg.size();

      exp_src.clear(); exp_src.resize(src.size(), TMatrix::Zero());
      z_src.clear(); z_src.resize(src.size(), 0.0);
      Array2D<TMatrix> exp_refs(src.size(), trg.size(), TMatrix::Zero());
      Array2D<double> z_refs(src.size(), trg.size(), 0.0);
      for (unsigned j = 0; j < trg.size(); ++j)
        trg_pos[trg[j]].insert(j);

      for (unsigned i = 0; i < src.size(); ++i) {
        const RVector& r_s = r_src[src[i]];
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
        trg_pos[trg[j]].clear();

      // model expectations for a single target generation with
      // uniform alignment prior
      // TODO: when using a non-uniform alignment, m_exp will be
      // a function of j (below)
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
          cerr << "TRG=" << TD::Convert(trg[j]) << endl;
          cerr << " LINE=" << lc << " (RANK=" << rank << "/" << size << ")" << endl;
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
      
      if (rank == 0 && (iter == (ITERATIONS - 1) || lc < 12)) { cerr << al << endl; }
    }
    if (flag && rank == 0) { cerr << endl; }

    double obj = 0;
    if (!SGD) {
      Flatten(g, &flat_g);
      obj = -likelihood;
#if HAVE_MPI
      rcv_grad.resize(flat_g.size(), 0.0);
      mpi::reduce(world, &flat_g[0], flat_g.size(), &rcv_grad[0], plus<double>(), 0);
      swap(flat_g, rcv_grad);
      rcv_grad.clear();

      double to = 0;
      mpi::reduce(world, obj, to, plus<double>(), 0);
      obj = to;
      double tlh = 0;
      mpi::reduce(world, likelihood, tlh, plus<double>(), 0);
      likelihood = tlh;
      double td = 0;
      mpi::reduce(world, denom, td, plus<double>(), 0);
      denom = td;
#endif
    }

    if (rank == 0) {
      double gn = 0;
      for (unsigned i = 0; i < flat_g.size(); ++i)
        gn += flat_g[i]*flat_g[i];
      const double base2_likelihood = likelihood / log(2);
      cerr << "  log_e likelihood: " << likelihood << endl;
      cerr << "  log_2 likelihood: " << base2_likelihood << endl;
      cerr << "     cross entropy: " << (-base2_likelihood / denom) << endl;
      cerr << "        perplexity: " << pow(2.0, -base2_likelihood / denom) << endl;
      cerr << "     gradient norm: " << sqrt(gn) << endl;
      if (!SGD) {
        if (has_l2) {
          const double r = ApplyRegularization(reg_strength,
                                               flat_t,
                                               &flat_g);
          obj += r;
          cerr << "    regularization: " << r << endl;
        }
        lbfgs->Optimize(obj, flat_g, &flat_t);
        converged = (lbfgs->HasConverged());
      }
    }
#ifdef HAVE_MPI
    mpi::broadcast(world, &flat_t[0], flat_t.size(), 0);
    mpi::broadcast(world, converged, 0);
#endif
  }
  if (rank == 0)
    cerr << "TRANSLATION MATRIX:" << endl << t << endl;
  return 0;
}

#endif

