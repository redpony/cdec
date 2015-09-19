#ifndef _DTRAIN_H_
#define _DTRAIN_H_

#include <iomanip>
#include <climits>
#include <string.h>

#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>
#include <boost/algorithm/string/regex.hpp>
#include <boost/program_options.hpp>

#include "decoder.h"
#include "ff_register.h"
#include "sentence_metadata.h"
#include "verbose.h"
#include "viterbi.h"

using namespace std;
namespace po = boost::program_options;

namespace dtrain
{

struct ScoredHyp
{
  vector<WordID>         w;
  SparseVector<weight_t> f;
  weight_t               model, gold;
  size_t                 rank;
};

inline void
PrintWordIDVec(vector<WordID>& v, ostream& os=cerr)
{
  for (size_t i = 0; i < v.size(); i++) {
    os << TD::Convert(v[i]);
    if (i < v.size()-1) os << " ";
  }
}

inline ostream& _np(ostream& out) { return out << resetiosflags(ios::showpos); }
inline ostream& _p(ostream& out)  { return out << setiosflags(ios::showpos); }
inline ostream& _p4(ostream& out) { return out << setprecision(4); }

bool
dtrain_init(int argc, char** argv, po::variables_map* conf)
{
  po::options_description opts("Configuration File Options");
  opts.add_options()
    ("bitext,b",           po::value<string>(),                                                      "bitext")
    ("decoder_conf,C",     po::value<string>(),                              "configuration file for decoder")
    ("iterations,T",       po::value<size_t>()->default_value(15),       "number of iterations T (per shard)")
    ("k",                  po::value<size_t>()->default_value(100),                      "size of kbest list")
    ("learning_rate,l",    po::value<weight_t>()->default_value(0.00001),                     "learning rate")
    ("l1_reg,r",           po::value<weight_t>()->default_value(0.),             "l1 regularization strength")
    ("margin,m",           po::value<weight_t>()->default_value(1.0),          "margin for margin perceptron")
    ("score,s",            po::value<string>()->default_value("chiang"),          "per-sentence BLEU approx.")
    ("N",                  po::value<size_t>()->default_value(4),                  "N for BLEU approximation")
    ("input_weights,w",    po::value<string>(),                                          "input weights file")
    ("average,a",          po::bool_switch()->default_value(true),                   "output average weights")
    ("keep,K",             po::bool_switch()->default_value(false),      "output a weight file per iteration")
    ("struct,S",           po::bool_switch()->default_value(false),           "structured SGD with hope/fear")
    ("output,o",           po::value<string>()->default_value("-"),     "output weights file, '-' for STDOUT")
    ("disable_learning,X", po::bool_switch()->default_value(false),                        "disable learning")
    ("output_data,D",      po::value<string>()->default_value(""), "output data to STDOUT; arg. is 'kbest', 'default' or 'all'")
    ("print_weights,P",    po::value<string>()->default_value("EgivenFCoherent SampleCountF CountEF MaxLexFgivenE MaxLexEgivenF IsSingletonF IsSingletonFE Glue WordPenalty PassThrough LanguageModel LanguageModel_OOV"),
                                                             "list of weights to print after each iteration");
  po::options_description clopts("Command Line Options");
  clopts.add_options()
    ("conf,c", po::value<string>(), "dtrain configuration file")
    ("help,h", po::bool_switch(),             "display options");
  opts.add(clopts);
  po::store(parse_command_line(argc, argv, opts), *conf);
  cerr << "dtrain" << endl << endl;
  if ((*conf)["help"].as<bool>()) {
    cerr << opts << endl;

    return false;
  }
  if (conf->count("conf")) {
    ifstream f((*conf)["conf"].as<string>().c_str());
    po::store(po::parse_config_file(f, opts), *conf);
  }
  po::notify(*conf);
  if (!conf->count("decoder_conf")) {
    cerr << "Missing decoder configuration." << endl;
    cerr << opts << endl;

    return false;
  }
  if (!conf->count("bitext")) {
    cerr << "No input given." << endl;
    cerr << opts << endl;

    return false;
  }
  if ((*conf)["output_data"].as<string>() != "") {
    if ((*conf)["output_data"].as<string>() != "kbest" &&
        (*conf)["output_data"].as<string>() != "default" &&
        (*conf)["output_data"].as<string>() != "all") {
      cerr << "Wrong 'output_data' argument: ";
      cerr << (*conf)["output_data"].as<string>() << endl;
      return false;
    }
  }

  return true;
}

} // namespace

#endif

