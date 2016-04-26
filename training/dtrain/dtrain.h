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

struct Hyp
{
  Hyp() {}
  Hyp(vector<WordID> w, SparseVector<weight_t> f, weight_t model, weight_t gold,
        size_t rank) : w(w), f(f), model(model), gold(gold), rank(rank) {}

  vector<WordID>         w;
  SparseVector<weight_t> f;
  weight_t               model, gold;
  size_t                 rank;
};

bool
dtrain_init(int argc,
            char** argv,
            po::variables_map* conf)
{
  po::options_description opts("Configuration File Options");
  opts.add_options()
    ("bitext,b",           po::value<string>(),
     "bitext, source and references in a single file [e ||| f]")
    ("decoder_conf,C",     po::value<string>(),
     "decoder configuration file")
    ("iterations,T",       po::value<size_t>()->default_value(15),
     "number of iterations T")
    ("k",                  po::value<size_t>()->default_value(100),
     "sample size per input (e.g. size of k-best lists)")
    ("unique_kbest",       po::bool_switch()->default_value(true),
     "unique k-best lists")
    ("forest_sample",      po::bool_switch()->default_value(false),
     "sample k hyptheses from forest instead of using k-best list")
    ("learning_rate,l",    po::value<weight_t>()->default_value(0.00001),
     "learning rate [only meaningful if margin>0 or input weights are given]")
    ("l1_reg,r",           po::value<weight_t>()->default_value(0.),
     "l1 regularization strength [see Tsuruoka, Tsujii and Ananiadou (2009)]")
    ("adadelta,D",         po::bool_switch()->default_value(false),
     "use AdaDelta dynamic learning rates")
    ("adadelta_decay",     po::value<weight_t>()->default_value(0.9),
     "decay for AdaDelta algorithm")
    ("adadelta_input",     po::value<string>()->default_value(""),
     "input for AdaDelta's parameters, two files: file.gradient, and file.update")
    ("adadelta_output",    po::value<string>()->default_value(""),
     "prefix for outputting AdaDelta's parameters")
    ("margin,m",           po::value<weight_t>()->default_value(1.0),
     "margin for margin perceptron [set =0 for standard perceptron]")
    ("cut,u",              po::value<weight_t>()->default_value(0.1),
     "use top/bottom 10% (default) of k-best as 'good' and 'bad' for pair sampling, 0 to use all pairs")
    ("adjust,A",           po::bool_switch()->default_value(false),
     "adjust cut for optimal pos. in k-best to cut")
    ("all,A",              po::bool_switch()->default_value(false),
     "update using all pairs, ignoring margin and threshold")
    ("score,s",            po::value<string>()->default_value("nakov"),
     "per-sentence BLEU (approx.)")
    ("nakov_fix",         po::value<weight_t>()->default_value(1.0),
     "add to reference length [see score.h]")
    ("chiang_decay",       po::value<weight_t>()->default_value(0.9),
     "decaying factor for Chiang's approx. BLEU")
    ("N",                  po::value<size_t>()->default_value(4),
     "N for BLEU approximation")
    ("input_weights,w",    po::value<string>(),
     "weights to initialize model")
    ("average,a",          po::bool_switch()->default_value(true),
     "output average weights")
    ("keep,K",             po::bool_switch()->default_value(false),
     "output a weight file per iteration [as weights.T.gz]")
    ("structured,S",       po::bool_switch()->default_value(false),
     "structured prediction objective [hope/fear] w/ SGD")
    ("pro_sampling",       po::bool_switch()->default_value(false),
     "updates from pairs selected as shown in Fig.4 of (Hopkins and May, 2011) [Gamma=max_pairs (default 5000), Xi=cut (default 50); threshold default 0.05]")
    ("threshold",          po::value<weight_t>()->default_value(0.),
     "(min.) threshold in terms of gold score for pair selection")
    ("max_pairs",
     po::value<size_t>()->default_value(numeric_limits<size_t>::max()),
     "max. number of updates/pairs")
    ("batch,B",            po::bool_switch()->default_value(false),
     "perform batch updates")
    ("output,o",           po::value<string>()->default_value("-"),
     "output weights file, '-' for STDOUT")
    ("disable_learning,X", po::bool_switch()->default_value(false),
     "fix model")
    ("output_updates,U",   po::value<string>()->default_value(""),
     "output updates (diff. vectors) [to filename]")
    ("output_raw,R",       po::value<string>()->default_value(""),
     "output raw data (e.g. k-best lists) [to filename]")
    ("stop_after",         po::value<size_t>()->default_value(numeric_limits<size_t>::max()),
     "only look at this number of segments")
    ("print_weights",      po::value<string>()->default_value("EgivenFCoherent SampleCountF CountEF MaxLexFgivenE MaxLexEgivenF IsSingletonF IsSingletonFE Glue WordPenalty PassThrough LanguageModel LanguageModel_OOV"),
     "list of weights to print after each iteration");
  po::options_description clopts("Command Line Options");
  clopts.add_options()
    ("conf,c", po::value<string>(), "dtrain configuration file")
    ("help,h", po::bool_switch(),             "display options");
  opts.add(clopts);
  po::store(parse_command_line(argc, argv, opts), *conf);
  cerr << "*dtrain*" << endl << endl;
  if ((*conf)["help"].as<bool>()) {
    cerr << setprecision(3) << opts << endl;

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
    cerr << "No input bitext." << endl;
    cerr << opts << endl;

    return false;
  }

  return true;
}

} // namespace

#endif

