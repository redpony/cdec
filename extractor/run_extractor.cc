#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "alignment.h"
#include "data_array.h"
#include "features/count_source_target.h"
#include "features/feature.h"
#include "features/is_source_singleton.h"
#include "features/is_source_target_singleton.h"
#include "features/max_lex_source_given_target.h"
#include "features/max_lex_target_given_source.h"
#include "features/sample_source_count.h"
#include "features/target_given_source_coherent.h"
#include "grammar.h"
#include "grammar_extractor.h"
#include "precomputation.h"
#include "rule.h"
#include "scorer.h"
#include "suffix_array.h"
#include "time_util.h"
#include "translation_table.h"

namespace fs = boost::filesystem;
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
    ("grammars,g", po::value<string>()->required(), "Grammars output path")
    ("frequent", po::value<int>()->default_value(100),
        "Number of precomputed frequent patterns")
    ("super_frequent", po::value<int>()->default_value(10),
        "Number of precomputed super frequent patterns")
    ("max_rule_span", po::value<int>()->default_value(15),
        "Maximum rule span")
    ("max_rule_symbols,l", po::value<int>()->default_value(5),
        "Maximum number of symbols (terminals + nontermals) in a rule")
    ("min_gap_size", po::value<int>()->default_value(1), "Minimum gap size")
    ("max_phrase_len", po::value<int>()->default_value(4),
        "Maximum frequent phrase length")
    ("max_nonterminals", po::value<int>()->default_value(2),
        "Maximum number of nonterminals in a rule")
    ("min_frequency", po::value<int>()->default_value(1000),
        "Minimum number of occurences for a pharse to be considered frequent")
    ("max_samples", po::value<int>()->default_value(300),
        "Maximum number of samples")
    // TODO(pauldb): Check if this works when set to false.
    ("tight_phrases", po::value<bool>()->default_value(true),
        "False if phrases may be loose (better, but slower)");

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

  Clock::time_point preprocess_start_time = Clock::now();
  cerr << "Reading source and target data..." << endl;
  Clock::time_point start_time = Clock::now();
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
  Clock::time_point stop_time = Clock::now();
  cerr << "Reading data took " << GetDuration(start_time, stop_time)
       << " seconds" << endl;

  cerr << "Creating source suffix array..." << endl;
  start_time = Clock::now();
  shared_ptr<SuffixArray> source_suffix_array =
      make_shared<SuffixArray>(source_data_array);
  stop_time = Clock::now();
  cerr << "Creating suffix array took "
       << GetDuration(start_time, stop_time) << " seconds" << endl;

  cerr << "Reading alignment..." << endl;
  start_time = Clock::now();
  shared_ptr<Alignment> alignment =
      make_shared<Alignment>(vm["alignment"].as<string>());
  stop_time = Clock::now();
  cerr << "Reading alignment took "
       << GetDuration(start_time, stop_time) << " seconds" << endl;

  cerr << "Precomputing collocations..." << endl;
  start_time = Clock::now();
  shared_ptr<Precomputation> precomputation = make_shared<Precomputation>(
      source_suffix_array,
      vm["frequent"].as<int>(),
      vm["super_frequent"].as<int>(),
      vm["max_rule_span"].as<int>(),
      vm["max_rule_symbols"].as<int>(),
      vm["min_gap_size"].as<int>(),
      vm["max_phrase_len"].as<int>(),
      vm["min_frequency"].as<int>());
  stop_time = Clock::now();
  cerr << "Precomputing collocations took "
       << GetDuration(start_time, stop_time) << " seconds" << endl;

  cerr << "Precomputing conditional probabilities..." << endl;
  start_time = Clock::now();
  shared_ptr<TranslationTable> table = make_shared<TranslationTable>(
      source_data_array, target_data_array, alignment);
  stop_time = Clock::now();
  cerr << "Precomputing conditional probabilities took "
       << GetDuration(start_time, stop_time) << " seconds" << endl;

  Clock::time_point preprocess_stop_time = Clock::now();
  cerr << "Overall preprocessing step took "
       << GetDuration(preprocess_start_time, preprocess_stop_time)
       << " seconds" << endl;

  cerr << "creating grammar extractor" << endl;

  Clock::time_point extraction_start_time = Clock::now();
  vector<shared_ptr<Feature> > features = {
      make_shared<TargetGivenSourceCoherent>(),
      make_shared<SampleSourceCount>(),
      make_shared<CountSourceTarget>(),
      make_shared<MaxLexSourceGivenTarget>(table),
      make_shared<MaxLexTargetGivenSource>(table),
      make_shared<IsSourceSingleton>(),
      make_shared<IsSourceTargetSingleton>()
  };
  shared_ptr<Scorer> scorer = make_shared<Scorer>(features);

  // TODO(pauldb): Add parallelization.
  GrammarExtractor extractor(
      source_suffix_array,
      target_data_array,
      alignment,
      precomputation,
      scorer,
      vm["min_gap_size"].as<int>(),
      vm["max_rule_span"].as<int>(),
      vm["max_nonterminals"].as<int>(),
      vm["max_rule_symbols"].as<int>(),
      vm["max_samples"].as<int>(),
      vm["tight_phrases"].as<bool>());

  // Release extra memory used by the initial precomputation.
  precomputation.reset();

  int grammar_id = 0;
  fs::path grammar_path = vm["grammars"].as<string>();
  if (!fs::is_directory(grammar_path)) {
    fs::create_directory(grammar_path);
  }

  string sentence, delimiter = "|||";
  while (getline(cin, sentence)) {
    string suffix = "";
    int position = sentence.find(delimiter);
    if (position != sentence.npos) {
      suffix = sentence.substr(position);
      sentence = sentence.substr(0, position);
    }

    Grammar grammar = extractor.GetGrammar(sentence);
    string file_name = "grammar." + to_string(grammar_id);
    fs::path grammar_file = grammar_path / file_name;
    ofstream output(grammar_file.c_str());
    output << grammar;

    cout << "<seg grammar=\"" << grammar_file << "\" id=\"" << grammar_id
         << "\"> " << sentence << " </seg> " << suffix << endl;
    ++grammar_id;
  }
  Clock::time_point extraction_stop_time = Clock::now();
  cerr << "Overall extraction step took "
       << GetDuration(extraction_start_time, extraction_stop_time)
       << " seconds" << endl;

  return 0;
}
