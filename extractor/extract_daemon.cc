#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <assert.h>
#include <cstring>

#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>

#include <ctime>

#include <nanomsg/nn.h>
#include <nanomsg/reqrep.h>

#include <boost/archive/binary_iarchive.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>
#if HAVE_OPEN_MP
 #include <omp.h>
#else
  const unsigned omp_get_num_threads() { return 1; }
#endif

#include "filelib.h"
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
fs::path GetGrammarFilePath(const fs::path& grammar_path, int file_number, bool use_zip) {
  string file_name = "grammar." + to_string(file_number) + (use_zip ? ".gz" : "");
  return grammar_path / file_name;
}

GrammarExtractor read_data(po::variables_map &vm, ofstream &log_file){
  Clock::time_point start_time = Clock::now();
  log_file << "Reading target data in binary format..." << endl;
  shared_ptr<DataArray> target_data_array = make_shared<DataArray>();
  ifstream target_fstream(vm["target"].as<string>());
  ar::binary_iarchive target_stream(target_fstream);
  target_stream >> *target_data_array;
  Clock::time_point end_time = Clock::now();
  log_file << "Reading target data took " << GetDuration(start_time, end_time)
       << " seconds" << endl;

  start_time = Clock::now();
  log_file << "Reading source suffix array in binary format..." << endl;
  shared_ptr<SuffixArray> source_suffix_array = make_shared<SuffixArray>();
  ifstream source_fstream(vm["source"].as<string>());
  ar::binary_iarchive source_stream(source_fstream);
  source_stream >> *source_suffix_array;
  end_time = Clock::now();
  log_file << "Reading source suffix array took "
       << GetDuration(start_time, end_time) << " seconds" << endl;

  start_time = Clock::now();
  log_file << "Reading alignment in binary format..." << endl;
  shared_ptr<Alignment> alignment = make_shared<Alignment>();
  ifstream alignment_fstream(vm["alignment"].as<string>());
  ar::binary_iarchive alignment_stream(alignment_fstream);
  alignment_stream >> *alignment;
  end_time = Clock::now();
  log_file << "Reading alignment took " << GetDuration(start_time, end_time)
       << " seconds" << endl;

  start_time = Clock::now();
  log_file << "Reading precomputation in binary format..." << endl;
  shared_ptr<Precomputation> precomputation = make_shared<Precomputation>();
  ifstream precomputation_fstream(vm["precomputation"].as<string>());
  ar::binary_iarchive precomputation_stream(precomputation_fstream);
  precomputation_stream >> *precomputation;
  end_time = Clock::now();
  log_file << "Reading precomputation took " << GetDuration(start_time, end_time)
       << " seconds" << endl;

  start_time = Clock::now();
  log_file << "Reading vocabulary in binary format..." << endl;
  shared_ptr<Vocabulary> vocabulary = make_shared<Vocabulary>();
  ifstream vocabulary_fstream(vm["vocabulary"].as<string>());
  ar::binary_iarchive vocabulary_stream(vocabulary_fstream);
  vocabulary_stream >> *vocabulary;
  end_time = Clock::now();
  log_file << "Reading vocabulary took " << GetDuration(start_time, end_time)
       << " seconds" << endl;

  start_time = Clock::now();
  log_file << "Reading translation table in binary format..." << endl;
  shared_ptr<TranslationTable> table = make_shared<TranslationTable>();
  ifstream ttable_fstream(vm["ttable"].as<string>());
  ar::binary_iarchive ttable_stream(ttable_fstream);
  ttable_stream >> *table;
  end_time = Clock::now();
  log_file << "Reading translation table took " << GetDuration(start_time, end_time)
       << " seconds" << endl;
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

  return extractor;
}

string extract_for_sentence(GrammarExtractor &extractor, po::variables_map &vm, const char* buf, int &i, bool use_zip){
  string sentence(buf);

  // Creates the grammars directory if it doesn't exist.
  fs::path grammar_path = vm["grammars"].as<string>();
  if (!fs::is_directory(grammar_path)) {
    fs::create_directory(grammar_path);
  }
  grammar_path = fs::canonical(grammar_path);

  // Extracts the grammar for each sentence and saves it to a file.
  bool leave_one_out = vm.count("leave_one_out");
  string suffix;
  int position = sentence.find("|||");
  if (position != sentence.npos) {
    suffix = sentence.substr(position);
    sentence = sentence.substr(0, position);
  }

  unordered_set<int> blacklisted_sentence_ids;
  if (leave_one_out) {
    blacklisted_sentence_ids.insert(i);
  }

  Grammar grammar = extractor.GetGrammar(
      sentence, blacklisted_sentence_ids);
  WriteFile wf(GetGrammarFilePath(grammar_path, i, use_zip).c_str());
  *wf.stream() << grammar;

  stringstream ss;
  ss << "<seg grammar=" << GetGrammarFilePath(grammar_path, i, use_zip) << " id=\""
     << i << "\"> " << sentence << " </seg> " << suffix << endl;

  return ss.str();
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
    ("grammars,g", po::value<string>()->required(), "Grammars (total) output path")
    ("gzip,z", "Gzip grammars")
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
        "(e.g. for extracting grammars for the training set")
    ("adress,a", po::value<string>()->required(), "Daemon adress")
    ("log_file_location,l", po::value<string>()->default_value("extract_daemon.log"),
        "user specified location for the log file");

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

  ofstream log_file;
  log_file.open(vm["log_file_location"].as<string>());

  int num_threads = vm["threads"].as<int>();

  const bool use_zip = vm.count("gzip");

  pid_t pid, sid;
  pid = fork();
  if (pid < 0) {
    exit(EXIT_FAILURE);
  }
  if (pid > 0) {
    exit(EXIT_SUCCESS);
  }
  sid = setsid();
  if (sid < 0) {
    exit(EXIT_FAILURE);
  }
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);

  log_file << "SID: " << sid << " (to kill me type \"kill " << sid << "\" on command line or \"killall run_extract_daemon\" to kill all instances)" << endl;
  log_file << "Starting up...Please do not send any requests yet" << endl;
  log_file << "Grammar extraction will use " << num_threads << " threads." << endl;

  string url = vm["adress"].as<string>();
  int socket = nn_socket(AF_SP, NN_REP);
  assert(socket >= 0);
  assert(nn_bind(socket, url.c_str()) >= 0);

  Clock::time_point read_start_time = Clock::now();

  GrammarExtractor extractor = read_data(vm, log_file);

  Clock::time_point read_end_time = Clock::now();
  log_file << "Total time spent loading data structures into memory: "
       << GetDuration(read_start_time, read_end_time) << " seconds" << endl;

  int count = 0;
  log_file << "Started up...";

  while(1){
    log_file << "Ready to receive requests" << endl;
    char *buf = NULL;
    int bytes = nn_recv(socket, &buf, NN_MSG, 0);
    assert (bytes >= 0);

    Clock::time_point extraction_start_time = Clock::now();
    log_file << "Extracting sentence number " << count
         << " since the start of this daemon" << endl;

    string output_sentence = extract_for_sentence(extractor, vm, buf, count, use_zip);
    const char *output_sentence_char = output_sentence.c_str();
    count++;
    int size_msg = strlen(output_sentence_char) + 1; // '\0' too
    bytes = nn_send(socket, output_sentence_char, size_msg, 0);
    assert(bytes == size_msg);

    Clock::time_point extraction_stop_time = Clock::now();
    log_file << "Sentence extraction step took "
         << GetDuration(extraction_start_time, extraction_stop_time)
         << " seconds" << endl;
  }
  return 0;
}
