#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>
#if HAVE_OPEN_MP
#include <omp.h>
#else
  const unsigned omp_get_num_threads() { return 1; }
#endif

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
#include "vocabulary.h"

namespace fs = boost::filesystem;
namespace po = boost::program_options;
using namespace std;
using namespace extractor;
using namespace features;

// Returns the file path in which a given grammar should be written.
fs::path GetGrammarFilePath(const fs::path& grammar_path, int file_number) {
  string file_name = "grammar." + to_string(file_number);
  return grammar_path / file_name;
}

int main(int argc, char** argv) {
  // Sets up the command line arguments map.
  int max_threads = 1;
  #pragma omp parallel
  max_threads = omp_get_num_threads();
  string threads_option = "Number of parallel threads for extraction "
                          "(max=" + to_string(max_threads) + ")";
  po::options_description desc("Command line options");
  desc.add_options()
    ("help,h", "Show available options")
    ("source,f", po::value<string>(), "Source language corpus")
    ("target,e", po::value<string>(), "Target language corpus")
    ("bitext,b", po::value<string>(), "Parallel text (source ||| target)")
    ("alignment,a", po::value<string>()->required(), "Bitext word alignment")
    ("grammars,g", po::value<string>()->required(), "Grammars output path")
    ("threads,t", po::value<int>()->default_value(1), threads_option.c_str())
    ("frequent", po::value<int>()->default_value(100),
        "Number of precomputed frequent patterns")
    ("super_frequent", po::value<int>()->default_value(10),
        "Number of precomputed super frequent patterns")
    ("max_rule_span", po::value<int>()->default_value(15),
        "Maximum rule span")
    ("max_rule_symbols", po::value<int>()->default_value(5),
        "Maximum number of symbols (terminals + nontermals) in a rule")
    ("min_gap_size", po::value<int>()->default_value(1), "Minimum gap size")
    ("max_phrase_len", po::value<int>()->default_value(4),
        "Maximum frequent phrase length")
    ("max_nonterminals", po::value<int>()->default_value(2),
        "Maximum number of nonterminals in a rule")
    ("min_frequency", po::value<int>()->default_value(1000),
        "Minimum number of occurrences for a pharse to be considered frequent")
    ("max_samples", po::value<int>()->default_value(300),
        "Maximum number of samples")
    ("tight_phrases", po::value<bool>()->default_value(true),
        "False if phrases may be loose (better, but slower)")
    ("leave_one_out", po::value<bool>()->zero_tokens(),
        "do leave-one-out estimation of grammars "
        "(e.g. for extracting grammars for the training set");

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);

  // Checks for the help option before calling notify, so the we don't get an
  // exception for missing required arguments.
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

  int num_threads = vm["threads"].as<int>();
  cerr << "Grammar extraction will use " << num_threads << " threads." << endl;

  // Reads the parallel corpus.
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

  // Constructs the suffix array for the source data.
  start_time = Clock::now();
  cerr << "Constructing source suffix array..." << endl;
  shared_ptr<SuffixArray> source_suffix_array =
      make_shared<SuffixArray>(source_data_array);
  stop_time = Clock::now();
  cerr << "Constructing suffix array took "
       << GetDuration(start_time, stop_time) << " seconds" << endl;

  // Reads the alignment.
  start_time = Clock::now();
  cerr << "Reading alignment..." << endl;
  shared_ptr<Alignment> alignment =
      make_shared<Alignment>(vm["alignment"].as<string>());
  stop_time = Clock::now();
  cerr << "Reading alignment took "
       << GetDuration(start_time, stop_time) << " seconds" << endl;

  shared_ptr<Vocabulary> vocabulary = make_shared<Vocabulary>();

  // Constructs an index storing the occurrences in the source data for each
  // frequent collocation.
  start_time = Clock::now();
  cerr << "Precomputing collocations..." << endl;
  shared_ptr<Precomputation> precomputation = make_shared<Precomputation>(
      vocabulary,
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

  // Constructs a table storing p(e | f) and p(f | e) for every pair of source
  // and target words.
  start_time = Clock::now();
  cerr << "Precomputing conditional probabilities..." << endl;
  shared_ptr<TranslationTable> table = make_shared<TranslationTable>(
      source_data_array, target_data_array, alignment);
  stop_time = Clock::now();
  cerr << "Precomputing conditional probabilities took "
       << GetDuration(start_time, stop_time) << " seconds" << endl;

  Clock::time_point preprocess_stop_time = Clock::now();
  cerr << "Overall preprocessing step took "
       << GetDuration(preprocess_start_time, preprocess_stop_time)
       << " seconds" << endl;

  Clock::time_point extraction_start_time = Clock::now();
  // Features used to score each grammar rule.
  vector<shared_ptr<Feature>> features = {
      make_shared<TargetGivenSourceCoherent>(),
      make_shared<SampleSourceCount>(),
      make_shared<CountSourceTarget>(),
      make_shared<MaxLexSourceGivenTarget>(table),
      make_shared<MaxLexTargetGivenSource>(table),
      make_shared<IsSourceSingleton>(),
      make_shared<IsSourceTargetSingleton>()
  };
  shared_ptr<Scorer> scorer = make_shared<Scorer>(features);

  // Sets up the grammar extractor.
  GrammarExtractor extractor(
      source_suffix_array,
      target_data_array,
      alignment,
      precomputation,
      scorer,
      vocabulary,
      vm["min_gap_size"].as<int>(),
      vm["max_rule_span"].as<int>(),
      vm["max_nonterminals"].as<int>(),
      vm["max_rule_symbols"].as<int>(),
      vm["max_samples"].as<int>(),
      vm["tight_phrases"].as<bool>());

  // Creates the grammars directory if it doesn't exist.
  fs::path grammar_path = vm["grammars"].as<string>();
  if (!fs::is_directory(grammar_path)) {
    fs::create_directory(grammar_path);
  }

  // Reads all sentences for which we extract grammar rules (the paralellization
  // is simplified if we read all sentences upfront).
  string sentence;
  vector<string> sentences;
  while (getline(cin, sentence)) {
    sentences.push_back(sentence);
  }

  // Extracts the grammar for each sentence and saves it to a file.
  bool leave_one_out = vm.count("leave_one_out");
  vector<string> suffixes(sentences.size());
  #pragma omp parallel for schedule(dynamic) num_threads(num_threads)
  for (size_t i = 0; i < sentences.size(); ++i) {
    string suffix;
    int position = sentences[i].find("|||");
    if (position != sentences[i].npos) {
      suffix = sentences[i].substr(position);
      sentences[i] = sentences[i].substr(0, position);
    }
    suffixes[i] = suffix;

    unordered_set<int> blacklisted_sentence_ids;
    if (leave_one_out) {
      blacklisted_sentence_ids.insert(i);
    }
    Grammar grammar = extractor.GetGrammar(
        sentences[i], blacklisted_sentence_ids);
    ofstream output(GetGrammarFilePath(grammar_path, i).c_str());
    output << grammar;
  }

  for (size_t i = 0; i < sentences.size(); ++i) {
    cout << "<seg grammar=" << GetGrammarFilePath(grammar_path, i) << " id=\""
         << i << "\"> " << sentences[i] << " </seg> " << suffixes[i] << endl;
  }

  Clock::time_point extraction_stop_time = Clock::now();
  cerr << "Overall extraction step took "
       << GetDuration(extraction_start_time, extraction_stop_time)
       << " seconds" << endl;

  return 0;
}
