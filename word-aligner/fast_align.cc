#include <iostream>
#include <cmath>
#include <utility>
#ifndef HAVE_OLD_CPP
# include <unordered_map>
#else
# include <tr1/unordered_map>
namespace std { using std::tr1::unordered_map; }
#endif

#include <boost/functional/hash.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "m.h"
#include "corpus_tools.h"
#include "stringlib.h"
#include "filelib.h"
#include "ttables.h"
#include "tdict.h"
#include "da.h"

namespace po = boost::program_options;
using namespace std;

bool InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("input,i",po::value<string>(),"Parallel corpus input file")
        ("reverse,r","Reverse estimation (swap source and target during training)")
        ("iterations,I",po::value<unsigned>()->default_value(5),"Number of iterations of EM training")
        //("bidir,b", "Run bidirectional alignment")
        ("favor_diagonal,d", "Use a static alignment distribution that assigns higher probabilities to alignments near the diagonal")
        ("prob_align_null", po::value<double>()->default_value(0.08), "When --favor_diagonal is set, what's the probability of a null alignment?")
        ("diagonal_tension,T", po::value<double>()->default_value(4.0), "How sharp or flat around the diagonal is the alignment distribution (<1 = flat >1 = sharp)")
        ("optimize_tension,o", "Optimize diagonal tension during EM")
        ("variational_bayes,v","Infer VB estimate of parameters under a symmetric Dirichlet prior")
        ("alpha,a", po::value<double>()->default_value(0.01), "Hyperparameter for optional Dirichlet prior")
        ("no_null_word,N","Do not generate from a null token")
        ("output_parameters,p", po::value<string>(), "Write model parameters to file")
        ("beam_threshold,t",po::value<double>()->default_value(-4),"When writing parameters, log_10 of beam threshold for writing parameter (-10000 to include everything, 0 max parameter only)")
        ("hide_training_alignments,H", "Hide training alignments (only useful if you want to use -x option and just compute testset statistics)")
        ("testset,x", po::value<string>(), "After training completes, compute the log likelihood of this set of sentence pairs under the learned model")
        ("no_add_viterbi,V","When writing model parameters, do not add Viterbi alignment points (may generate a grammar where some training sentence pairs are unreachable)")
		("force_align,f",po::value<string>(), "Load previously written parameters to 'force align' input. Set --diagonal_tension and --mean_srclen_multiplier as estimated during training.")
		("mean_srclen_multiplier,m",po::value<double>()->default_value(1), "When --force_align, use this source length multiplier");
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

  if (conf->count("help") || conf->count("input") == 0) {
    cerr << "Usage " << argv[0] << " [OPTIONS] -i corpus.fr-en\n";
    cerr << dcmdline_options << endl;
    return false;
  }
  return true;
}

int main(int argc, char** argv) {
  po::variables_map conf;
  if (!InitCommandLine(argc, argv, &conf)) return 1;
  const string fname = conf["input"].as<string>();
  const bool reverse = conf.count("reverse") > 0;
  const int ITERATIONS = (conf.count("force_align")) ? 0 : conf["iterations"].as<unsigned>();
  const double BEAM_THRESHOLD = pow(10.0, conf["beam_threshold"].as<double>());
  const bool use_null = (conf.count("no_null_word") == 0);
  const WordID kNULL = TD::Convert("<eps>");
  const bool add_viterbi = (conf.count("no_add_viterbi") == 0);
  const bool variational_bayes = (conf.count("variational_bayes") > 0);
  const bool output_parameters = (conf.count("force_align")) ? false : conf.count("output_parameters");
  double diagonal_tension = conf["diagonal_tension"].as<double>();
  bool optimize_tension = conf.count("optimize_tension");
  bool hide_training_alignments = (conf.count("hide_training_alignments") > 0);
  const bool write_alignments = (conf.count("force_align")) ? true : !hide_training_alignments;
  string testset;
  if (conf.count("testset")) testset = conf["testset"].as<string>();
  if (conf.count("force_align")) testset = fname;
  double prob_align_null = conf["prob_align_null"].as<double>();
  double prob_align_not_null = 1.0 - prob_align_null;
  const double alpha = conf["alpha"].as<double>();
  const bool favor_diagonal = conf.count("favor_diagonal");
  if (variational_bayes && alpha <= 0.0) {
    cerr << "--alpha must be > 0\n";
    return 1;
  }
  
  
  TTable s2t, t2s;
  TTable::Word2Word2Double s2t_viterbi;
  unordered_map<pair<short, short>, unsigned, boost::hash<pair<short, short> > > size_counts;
  double tot_len_ratio = 0;
  double mean_srclen_multiplier = 0;
  vector<double> probs;
  
  if (conf.count("force_align")) {
	// load model parameters
	ReadFile s2t_f(conf["force_align"].as<string>());
	s2t.DeserializeLogProbsFromText(s2t_f.stream());
	mean_srclen_multiplier = conf["mean_srclen_multiplier"].as<double>();
  }
  
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
    vector<WordID> src, trg;
    double c0 = 0;
    double emp_feat = 0;
    double toks = 0;
    while(true) {
      getline(in, line);
      if (!in) break;
      ++lc;
      if (lc % 1000 == 0) { cerr << '.'; flag = true; }
      if (lc %50000 == 0) { cerr << " [" << lc << "]\n" << flush; flag = false; }
      src.clear(); trg.clear();
      CorpusTools::ReadLine(line, &src, &trg);
      if (reverse) swap(src, trg);
      if (src.size() == 0 || trg.size() == 0) {
        cerr << "Error: " << lc << "\n" << line << endl;
        return 1;
      }
      if (iter == 0)
        tot_len_ratio += static_cast<double>(trg.size()) / static_cast<double>(src.size());
      denom += trg.size();
      probs.resize(src.size() + 1);
      if (iter == 0)
        ++size_counts[make_pair<short,short>(trg.size(), src.size())];
      bool first_al = true;  // used for write_alignments
      toks += trg.size();
      for (unsigned j = 0; j < trg.size(); ++j) {
        const WordID& f_j = trg[j];
        double sum = 0;
        double prob_a_i = 1.0 / (src.size() + use_null);  // uniform (model 1)
        if (use_null) {
          if (favor_diagonal) prob_a_i = prob_align_null;
          probs[0] = s2t.prob(kNULL, f_j) * prob_a_i;
          sum += probs[0];
        }
        double az = 0;
        if (favor_diagonal)
          az = DiagonalAlignment::ComputeZ(j+1, trg.size(), src.size(), diagonal_tension) / prob_align_not_null;
        for (unsigned i = 1; i <= src.size(); ++i) {
          if (favor_diagonal)
            prob_a_i = DiagonalAlignment::UnnormalizedProb(j + 1, i, trg.size(), src.size(), diagonal_tension) / az;
          probs[i] = s2t.prob(src[i-1], f_j) * prob_a_i;
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
            for (unsigned i = 1; i <= src.size(); ++i) {
              if (probs[i] > max_p) {
                max_index = i;
                max_p = probs[i];
                max_i = src[i-1];
              }
            }
            if (!hide_training_alignments && write_alignments) {
              if (max_index > 0) {
                if (first_al) first_al = false; else cout << ' ';
                if (reverse)
                  cout << j << '-' << (max_index - 1);
                else
                  cout << (max_index - 1) << '-' << j;
              }
            }
            s2t_viterbi[max_i][f_j] = 1.0;
          }
        } else {
          if (use_null) {
            double count = probs[0] / sum;
            c0 += count;
            s2t.Increment(kNULL, f_j, count);
          }
          for (unsigned i = 1; i <= src.size(); ++i) {
            const double p = probs[i] / sum;
            s2t.Increment(src[i-1], f_j, p);
            emp_feat += DiagonalAlignment::Feature(j, i, trg.size(), src.size()) * p;
          }
        }
        likelihood += log(sum);
      }
      if (write_alignments && final_iteration && !hide_training_alignments) cout << endl;
    }

    // log(e) = 1.0
    double base2_likelihood = likelihood / log(2);

    if (flag) { cerr << endl; }
    if (iter == 0) {
      mean_srclen_multiplier = tot_len_ratio / lc;
      cerr << "expected target length = source length * " << mean_srclen_multiplier << endl;
    }
    emp_feat /= toks;
    cerr << "  log_e likelihood: " << likelihood << endl;
    cerr << "  log_2 likelihood: " << base2_likelihood << endl;
    cerr << "     cross entropy: " << (-base2_likelihood / denom) << endl;
    cerr << "        perplexity: " << pow(2.0, -base2_likelihood / denom) << endl;
    cerr << "      posterior p0: " << c0 / toks << endl;
    cerr << " posterior al-feat: " << emp_feat << endl;
    //cerr << "     model tension: " << mod_feat / toks << endl;
    cerr << "       size counts: " << size_counts.size() << endl;
    if (!final_iteration) {
      if (favor_diagonal && optimize_tension && iter > 0) {
        for (int ii = 0; ii < 8; ++ii) {
          double mod_feat = 0;
          unordered_map<pair<short,short>,unsigned,boost::hash<pair<short, short> > >::iterator it = size_counts.begin();
          for(; it != size_counts.end(); ++it) {
            const pair<short,short>& p = it->first;
            for (short j = 1; j <= p.first; ++j)
              mod_feat += it->second * DiagonalAlignment::ComputeDLogZ(j, p.first, p.second, diagonal_tension);
          }
          mod_feat /= toks;
          cerr << "  " << ii + 1 << "  model al-feat: " << mod_feat << " (tension=" << diagonal_tension << ")\n";
          diagonal_tension += (emp_feat - mod_feat) * 20.0;
          if (diagonal_tension <= 0.1) diagonal_tension = 0.1;
          if (diagonal_tension > 14) diagonal_tension = 14;
        }
        cerr << "     final tension: " << diagonal_tension << endl;
      }
      if (variational_bayes)
        s2t.NormalizeVB(alpha);
      else
        s2t.Normalize();
      //prob_align_null *= 0.8; // XXX
      //prob_align_null += (c0 / toks) * 0.2;
      prob_align_not_null = 1.0 - prob_align_null;
    }
  }
  if (testset.size()) {
    ReadFile rf(testset);
    istream& in = *rf.stream();
    int lc = 0;
    double tlp = 0;
    string line;
    while (getline(in, line)) {
      ++lc;
      vector<WordID> src, trg;
      CorpusTools::ReadLine(line, &src, &trg);
      cout << TD::GetString(src) << " ||| " << TD::GetString(trg) << " |||";
      if (reverse) swap(src, trg);
      double log_prob = Md::log_poisson(trg.size(), 0.05 + src.size() * mean_srclen_multiplier);

      // compute likelihood
      for (unsigned j = 0; j < trg.size(); ++j) {
        const WordID& f_j = trg[j];
        double sum = 0;
        int a_j = 0;
        double max_pat = 0;
        double prob_a_i = 1.0 / (src.size() + use_null);  // uniform (model 1)
        if (use_null) {
          if (favor_diagonal) prob_a_i = prob_align_null;
          max_pat = s2t.prob(kNULL, f_j) * prob_a_i;
          sum += max_pat;
        }
        double az = 0;
        if (favor_diagonal)
          az = DiagonalAlignment::ComputeZ(j+1, trg.size(), src.size(), diagonal_tension) / prob_align_not_null;
        for (unsigned i = 1; i <= src.size(); ++i) {
          if (favor_diagonal)
            prob_a_i = DiagonalAlignment::UnnormalizedProb(j + 1, i, trg.size(), src.size(), diagonal_tension) / az;
          double pat = s2t.prob(src[i-1], f_j) * prob_a_i;
          if (pat > max_pat) { max_pat = pat; a_j = i; }
          sum += pat;
        }
        log_prob += log(sum);
        if (write_alignments) {
          if (a_j > 0) {
            cout << ' ';
            if (reverse)
              cout << j << '-' << (a_j - 1);
            else
              cout << (a_j - 1) << '-' << j;
          }
        }
      }
      tlp += log_prob;
      cout << " ||| " << log_prob << endl << flush;
    } // loop over test set sentences
    cerr << "TOTAL LOG PROB " << tlp << endl;
  }

  if (output_parameters) {
    WriteFile params_out(conf["output_parameters"].as<string>());
    for (TTable::Word2Word2Double::iterator ei = s2t.ttable.begin(); ei != s2t.ttable.end(); ++ei) {
      const TTable::Word2Double& cpd = ei->second;
      const TTable::Word2Double& vit = s2t_viterbi[ei->first];
      const string& esym = TD::Convert(ei->first);
      double max_p = -1;
      for (TTable::Word2Double::const_iterator fi = cpd.begin(); fi != cpd.end(); ++fi)
        if (fi->second > max_p) max_p = fi->second;
      const double threshold = max_p * BEAM_THRESHOLD;
      for (TTable::Word2Double::const_iterator fi = cpd.begin(); fi != cpd.end(); ++fi) {
        if (fi->second > threshold || (vit.find(fi->first) != vit.end())) {
          *params_out << esym << ' ' << TD::Convert(fi->first) << ' ' << log(fi->second) << endl;
        }
      } 
    }
  }
  return 0;
}

