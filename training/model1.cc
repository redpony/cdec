#include <iostream>
#include <cmath>

#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "m.h"
#include "lattice.h"
#include "stringlib.h"
#include "filelib.h"
#include "ttables.h"
#include "tdict.h"

namespace po = boost::program_options;
using namespace std;

bool InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("iterations,i",po::value<unsigned>()->default_value(5),"Number of iterations of EM training")
        ("beam_threshold,t",po::value<double>()->default_value(-4),"log_10 of beam threshold (-10000 to include everything, 0 max)")
        ("no_null_word,N","Do not generate from the null token")
        ("write_alignments,A", "Write alignments instead of parameters")
        ("favor_diagonal,d", "Use a static alignment distribution that assigns higher probabilities to alignments near the diagonal")
        ("diagonal_tension,T", po::value<double>()->default_value(4.0), "How sharp or flat around the diagonal is the alignment distribution (<1 = flat >1 = sharp)")
        ("prob_align_null", po::value<double>()->default_value(0.08), "When --favor_diagonal is set, what's the probability of a null alignment?")
        ("variational_bayes,v","Add a symmetric Dirichlet prior and infer VB estimate of weights")
        ("testset,x", po::value<string>(), "After training completes, compute the log likelihood of this set of sentence pairs under the learned model")
        ("alpha,a", po::value<double>()->default_value(0.01), "Hyperparameter for optional Dirichlet prior")
        ("no_add_viterbi,V","Do not add Viterbi alignment points (may generate a grammar where some training sentence pairs are unreachable)");
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
    cerr << "Usage " << argv[0] << " [OPTIONS] corpus.fr-en\n";
    cerr << dcmdline_options << endl;
    return false;
  }
  return true;
}

int main(int argc, char** argv) {
  po::variables_map conf;
  if (!InitCommandLine(argc, argv, &conf)) return 1;
  const string fname = argv[argc - 1];
  const int ITERATIONS = conf["iterations"].as<unsigned>();
  const double BEAM_THRESHOLD = pow(10.0, conf["beam_threshold"].as<double>());
  const bool use_null = (conf.count("no_null_word") == 0);
  const WordID kNULL = TD::Convert("<eps>");
  const bool add_viterbi = (conf.count("no_add_viterbi") == 0);
  const bool variational_bayes = (conf.count("variational_bayes") > 0);
  const bool write_alignments = (conf.count("write_alignments") > 0);
  const double diagonal_tension = conf["diagonal_tension"].as<double>();
  const double prob_align_null = conf["prob_align_null"].as<double>();
  string testset;
  if (conf.count("testset")) testset = conf["testset"].as<string>();
  const double prob_align_not_null = 1.0 - prob_align_null;
  const double alpha = conf["alpha"].as<double>();
  const bool favor_diagonal = conf.count("favor_diagonal");
  if (variational_bayes && alpha <= 0.0) {
    cerr << "--alpha must be > 0\n";
    return 1;
  }

  TTable tt;
  TTable::Word2Word2Double was_viterbi;
  double tot_len_ratio = 0;
  double mean_srclen_multiplier = 0;
  vector<double> unnormed_a_i;
  for (int iter = 0; iter < ITERATIONS; ++iter) {
    const bool final_iteration = (iter == (ITERATIONS - 1));
    cerr << "ITERATION " << (iter + 1) << (final_iteration ? " (FINAL)" : "") << endl;
    ReadFile rf(fname);
    istream& in = *rf.stream();
    double likelihood = 0;
    double denom = 0.0;
    int lc = 0;
    bool flag = false;
    string line;
    string ssrc, strg;
    while(true) {
      getline(in, line);
      if (!in) break;
      ++lc;
      if (lc % 1000 == 0) { cerr << '.'; flag = true; }
      if (lc %50000 == 0) { cerr << " [" << lc << "]\n" << flush; flag = false; }
      ParseTranslatorInput(line, &ssrc, &strg);
      Lattice src, trg;
      LatticeTools::ConvertTextToLattice(ssrc, &src);
      LatticeTools::ConvertTextToLattice(strg, &trg);
      if (src.size() == 0 || trg.size() == 0) {
        cerr << "Error: " << lc << "\n" << line << endl;
        assert(src.size() > 0);
        assert(trg.size() > 0);
      }
      if (src.size() > unnormed_a_i.size())
        unnormed_a_i.resize(src.size());
      if (iter == 0)
        tot_len_ratio += static_cast<double>(trg.size()) / static_cast<double>(src.size());
      denom += trg.size();
      vector<double> probs(src.size() + 1);
      bool first_al = true;  // used for write_alignments
      for (int j = 0; j < trg.size(); ++j) {
        const WordID& f_j = trg[j][0].label;
        double sum = 0;
        const double j_over_ts = double(j) / trg.size();
        double prob_a_i = 1.0 / (src.size() + use_null);  // uniform (model 1)
        if (use_null) {
          if (favor_diagonal) prob_a_i = prob_align_null;
          probs[0] = tt.prob(kNULL, f_j) * prob_a_i;
          sum += probs[0];
        }
        double az = 0;
        if (favor_diagonal) {
          for (int ta = 0; ta < src.size(); ++ta) {
            unnormed_a_i[ta] = exp(-fabs(double(ta) / src.size() - j_over_ts) * diagonal_tension);
            az += unnormed_a_i[ta];
          }
          az /= prob_align_not_null;
        }
        for (int i = 1; i <= src.size(); ++i) {
          if (favor_diagonal)
            prob_a_i = unnormed_a_i[i-1] / az;
          probs[i] = tt.prob(src[i-1][0].label, f_j) * prob_a_i;
          sum += probs[i];
        }
        if (final_iteration) {
          if (add_viterbi || write_alignments) {
            WordID max_i = 0;
            double max_p = -1;
            int max_index = -1;
            if (use_null) {
              max_i = kNULL;
              max_index = 0;
              max_p = probs[0];
            }
            for (int i = 1; i <= src.size(); ++i) {
              if (probs[i] > max_p) {
                max_index = i;
                max_p = probs[i];
                max_i = src[i-1][0].label;
              }
            }
            if (write_alignments) {
              if (max_index > 0) {
                if (first_al) first_al = false; else cout << ' ';
                cout << (max_index - 1) << "-" << j;
              }
            }
            was_viterbi[max_i][f_j] = 1.0;
          }
        } else {
          if (use_null)
            tt.Increment(kNULL, f_j, probs[0] / sum);
          for (int i = 1; i <= src.size(); ++i)
            tt.Increment(src[i-1][0].label, f_j, probs[i] / sum);
        }
        likelihood += log(sum);
      }
      if (write_alignments && final_iteration) cout << endl;
    }

    // log(e) = 1.0
    double base2_likelihood = likelihood / log(2);

    if (flag) { cerr << endl; }
    if (iter == 0) {
      mean_srclen_multiplier = tot_len_ratio / lc;
      cerr << "expected target length = source length * " << mean_srclen_multiplier << endl;
    }
    cerr << "  log_e likelihood: " << likelihood << endl;
    cerr << "  log_2 likelihood: " << base2_likelihood << endl;
    cerr << "   cross entropy: " << (-base2_likelihood / denom) << endl;
    cerr << "      perplexity: " << pow(2.0, -base2_likelihood / denom) << endl;
    if (!final_iteration) {
      if (variational_bayes)
        tt.NormalizeVB(alpha);
      else
        tt.Normalize();
    }
  }
  if (testset.size()) {
    ReadFile rf(testset);
    istream& in = *rf.stream();
    int lc = 0;
    double tlp = 0;
    string ssrc, strg, line;
    while (getline(in, line)) {
      ++lc;
      ParseTranslatorInput(line, &ssrc, &strg);
      Lattice src, trg;
      LatticeTools::ConvertTextToLattice(ssrc, &src);
      LatticeTools::ConvertTextToLattice(strg, &trg);
      double log_prob = Md::log_poisson(trg.size(), 0.05 + src.size() * mean_srclen_multiplier);
      if (src.size() > unnormed_a_i.size())
        unnormed_a_i.resize(src.size());

      // compute likelihood
      for (int j = 0; j < trg.size(); ++j) {
        const WordID& f_j = trg[j][0].label;
        double sum = 0;
        const double j_over_ts = double(j) / trg.size();
        double prob_a_i = 1.0 / (src.size() + use_null);  // uniform (model 1)
        if (use_null) {
          if (favor_diagonal) prob_a_i = prob_align_null;
          sum += tt.prob(kNULL, f_j) * prob_a_i;
        }
        double az = 0;
        if (favor_diagonal) {
          for (int ta = 0; ta < src.size(); ++ta) {
            unnormed_a_i[ta] = exp(-fabs(double(ta) / src.size() - j_over_ts) * diagonal_tension);
            az += unnormed_a_i[ta];
          }
          az /= prob_align_not_null;
        }
        for (int i = 1; i <= src.size(); ++i) {
          if (favor_diagonal)
            prob_a_i = unnormed_a_i[i-1] / az;
          sum += tt.prob(src[i-1][0].label, f_j) * prob_a_i;
        }
        log_prob += log(sum);
      }
      tlp += log_prob;
      cerr << ssrc << " ||| " << strg << " ||| " << log_prob << endl;
    }
    cerr << "TOTAL LOG PROB " << tlp << endl;
  }

  if (write_alignments) return 0;

  for (TTable::Word2Word2Double::iterator ei = tt.ttable.begin(); ei != tt.ttable.end(); ++ei) {
    const TTable::Word2Double& cpd = ei->second;
    const TTable::Word2Double& vit = was_viterbi[ei->first];
    const string& esym = TD::Convert(ei->first);
    double max_p = -1;
    for (TTable::Word2Double::const_iterator fi = cpd.begin(); fi != cpd.end(); ++fi)
      if (fi->second > max_p) max_p = fi->second;
    const double threshold = max_p * BEAM_THRESHOLD;
    for (TTable::Word2Double::const_iterator fi = cpd.begin(); fi != cpd.end(); ++fi) {
      if (fi->second > threshold || (vit.count(fi->first) > 0)) {
        cout << esym << ' ' << TD::Convert(fi->first) << ' ' << log(fi->second) << endl;
      }
    } 
  }
  return 0;
}

