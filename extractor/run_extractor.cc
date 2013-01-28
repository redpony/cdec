#include <iostream>
#include <string>

#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "alignment.h"
#include "data_array.h"
#include "grammar_extractor.h"
#include "precomputation.h"
#include "suffix_array.h"
#include "translation_table.h"

namespace po = boost::program_options;
using namespace std;

int main(int argc, char** argv) {
  // TODO(pauldb): Also take arguments from config file.
  po::options_description desc("Command line options");
  desc.add_options()
    ("help,h", "Show available options")
    ("source,f", po::value<string>(), "Source language corpus")
    ("target,e", po::value<string>(), "Target language corpus")
    ("bitext,b", po::value<string>(), "Parallel text (source ||| target)")
    ("alignment,a", po::value<string>()->required(), "Bitext word alignment")
    ("frequent", po::value<int>()->default_value(100),
        "Number of precomputed frequent patterns")
    ("super_frequent", po::value<int>()->default_value(10),
        "Number of precomputed super frequent patterns")
    ("max_rule_span,s", po::value<int>()->default_value(15),
        "Maximum rule span")
    ("max_rule_symbols,l", po::value<int>()->default_value(5),
        "Maximum number of symbols (terminals + nontermals) in a rule")
    ("min_gap_size,g", po::value<int>()->default_value(1), "Minimum gap size")
    ("max_phrase_len,p", po::value<int>()->default_value(4),
        "Maximum frequent phrase length")
    ("max_nonterminals", po::value<int>()->default_value(2),
        "Maximum number of nonterminals in a rule")
    ("min_frequency", po::value<int>()->default_value(1000),
        "Minimum number of occurences for a pharse to be considered frequent")
    ("baeza_yates", po::value<bool>()->default_value(true),
        "Use double binary search");

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);

  // Check for help argument before notify, so we don't need to pass in the
  // required parameters.
  if (vm.count("help")) {
    cout << desc << endl;
    return 0;
  }

  po::notify(vm);

  if (!((vm.count("source") && vm.count("target")) || vm.count("bitext"))) {
    cerr << "A paralel corpus is required. "
         << "Use -f (source) with -e (target) or -b (bitext)."
         << endl;
    return 1;
  }

  shared_ptr<DataArray> source_data_array, target_data_array;
  if (vm.count("bitext")) {
    source_data_array = make_shared<DataArray>(
        vm["bitext"].as<string>(), SOURCE);
    target_data_array = make_shared<DataArray>(
        vm["bitext"].as<string>(), TARGET);
  } else {
    source_data_array = make_shared<DataArray>(vm["source"].as<string>());
    target_data_array = make_shared<DataArray>(vm["target"].as<string>());
  }
  shared_ptr<SuffixArray> source_suffix_array =
      make_shared<SuffixArray>(source_data_array);


  Alignment alignment(vm["alignment"].as<string>());

  Precomputation precomputation(
      source_suffix_array,
      vm["frequent"].as<int>(),
      vm["super_frequent"].as<int>(),
      vm["max_rule_span"].as<int>(),
      vm["max_rule_symbols"].as<int>(),
      vm["min_gap_size"].as<int>(),
      vm["max_phrase_len"].as<int>(),
      vm["min_frequency"].as<int>());

  TranslationTable table(source_data_array, target_data_array, alignment);

  // TODO(pauldb): Add parallelization.
  GrammarExtractor extractor(
      source_suffix_array,
      target_data_array,
      alignment,
      precomputation,
      vm["min_gap_size"].as<int>(),
      vm["max_rule_span"].as<int>(),
      vm["max_nonterminals"].as<int>(),
      vm["max_rule_symbols"].as<int>(),
      vm["baeza_yates"].as<bool>());

  string sentence;
  while (getline(cin, sentence)) {
    extractor.GetGrammar(sentence);
  }

  return 0;
}
