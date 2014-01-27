#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <boost/archive/binary_iarchive.hpp>
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

namespace ar = boost::archive;
namespace fs = boost::filesystem;
namespace po = boost::program_options;
using namespace extractor;
using namespace features;
using namespace std;

// Returns the file path in which a given grammar should be written.
fs::path GetGrammarFilePath(const fs::path& grammar_path, int file_number) {
  string file_name = "grammar." + to_string(file_number);
  return grammar_path / file_name;
}

int main(int argc, char** argv) {
  po::options_description general_options("General options");
  int max_threads = 1;
  #pragma omp parallel
  max_threads = omp_get_num_threads();
  string threads_option = "Number of threads used for grammar extraction "
                          "max(" + to_string(max_threads) + ")";
  general_options.add_options()
    ("threads,t", po::value<int>()->required()->default_value(1),
     threads_option.c_str())
    ("grammars,g", po::value<string>()->required(), "Grammars output path")
    ("max_rule_span", po::value<int>()->default_value(15),
        "Maximum rule span")
    ("max_rule_symbols", po::value<int>()->default_value(5),
        "Maximum number of symbols (terminals + nontermals) in a rule")
    ("min_gap_size", po::value<int>()->default_value(1), "Minimum gap size")
    ("max_nonterminals", po::value<int>()->default_value(2),
        "Maximum number of nonterminals in a rule")
    ("max_samples", po::value<int>()->default_value(300),
        "Maximum number of samples")
    ("tight_phrases", po::value<bool>()->default_value(true),
        "False if phrases may be loose (better, but slower)")
    ("leave_one_out", po::value<bool>()->zero_tokens(),
        "do leave-one-out estimation of grammars "
        "(e.g. for extracting grammars for the training set");

  po::options_description cmdline_options("Command line options");
  cmdline_options.add_options()
    ("help", "Show available options")
    ("config,c", po::value<string>()->required(), "Path to config file");
  cmdline_options.add(general_options);

  po::options_description config_options("Config file options");
  config_options.add_options()
    ("target", po::value<string>()->required(),
        "Path to target data file in binary format")
    ("source", po::value<string>()->required(),
        "Path to source suffix array file in binary format")
    ("alignment", po::value<string>()->required(),
        "Path to alignment file in binary format")
    ("precomputation", po::value<string>()->required(),
        "Path to precomputation file in binary format")
    ("vocabulary", po::value<string>()->required(),
        "Path to vocabulary file in binary format")
    ("ttable", po::value<string>()->required(),
        "Path to translation table in binary format");
  config_options.add(general_options);

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, cmdline_options), vm);
  if (vm.count("help")) {
    po::options_description all_options;
    all_options.add(cmdline_options).add(config_options);
    cout << all_options << endl;
    return 0;
  }

  po::notify(vm);

  ifstream config_stream(vm["config"].as<string>());
  po::store(po::parse_config_file(config_stream, config_options), vm);
  po::notify(vm);

  int num_threads = vm["threads"].as<int>();
  cerr << "Grammar extraction will use " << num_threads << " threads." << endl;

  Clock::time_point read_start_time = Clock::now();

  Clock::time_point start_time = Clock::now();
  cerr << "Reading target data in binary format..." << endl;
  shared_ptr<DataArray> target_data_array = make_shared<DataArray>();
  ifstream target_fstream(vm["target"].as<string>());
  ar::binary_iarchive target_stream(target_fstream);
  target_stream >> *target_data_array;
  Clock::time_point end_time = Clock::now();
  cerr << "Reading target data took " << GetDuration(start_time, end_time)
       << " seconds" << endl;

  start_time = Clock::now();
  cerr << "Reading source suffix array in binary format..." << endl;
  shared_ptr<SuffixArray> source_suffix_array = make_shared<SuffixArray>();
  ifstream source_fstream(vm["source"].as<string>());
  ar::binary_iarchive source_stream(source_fstream);
  source_stream >> *source_suffix_array;
  end_time = Clock::now();
  cerr << "Reading source suffix array took "
       << GetDuration(start_time, end_time) << " seconds" << endl;

  start_time = Clock::now();
  cerr << "Reading alignment in binary format..." << endl;
  shared_ptr<Alignment> alignment = make_shared<Alignment>();
  ifstream alignment_fstream(vm["alignment"].as<string>());
  ar::binary_iarchive alignment_stream(alignment_fstream);
  alignment_stream >> *alignment;
  end_time = Clock::now();
  cerr << "Reading alignment took " << GetDuration(start_time, end_time)
       << " seconds" << endl;

  start_time = Clock::now();
  cerr << "Reading precomputation in binary format..." << endl;
  shared_ptr<Precomputation> precomputation = make_shared<Precomputation>();
  ifstream precomputation_fstream(vm["precomputation"].as<string>());
  ar::binary_iarchive precomputation_stream(precomputation_fstream);
  precomputation_stream >> *precomputation;
  end_time = Clock::now();
  cerr << "Reading precomputation took " << GetDuration(start_time, end_time)
       << " seconds" << endl;

  start_time = Clock::now();
  cerr << "Reading vocabulary in binary format..." << endl;
  shared_ptr<Vocabulary> vocabulary = make_shared<Vocabulary>();
  ifstream vocabulary_fstream(vm["vocabulary"].as<string>());
  ar::binary_iarchive vocabulary_stream(vocabulary_fstream);
  vocabulary_stream >> *vocabulary;
  end_time = Clock::now();
  cerr << "Reading vocabulary took " << GetDuration(start_time, end_time)
       << " seconds" << endl;

  start_time = Clock::now();
  cerr << "Reading translation table in binary format..." << endl;
  shared_ptr<TranslationTable> table = make_shared<TranslationTable>();
  ifstream ttable_fstream(vm["ttable"].as<string>());
  ar::binary_iarchive ttable_stream(ttable_fstream);
  ttable_stream >> *table;
  end_time = Clock::now();
  cerr << "Reading translation table took " << GetDuration(start_time, end_time)
       << " seconds" << endl;

  Clock::time_point read_end_time = Clock::now();
  cerr << "Total time spent loading data structures into memory: "
       << GetDuration(read_start_time, read_end_time) << " seconds" << endl;

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
  vector<string> suffixes(sentences.size());
  bool leave_one_out = vm.count("leave_one_out");
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
