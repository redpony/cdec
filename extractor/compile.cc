#include <fstream>
#include <iostream>
#include <string>

#include <boost/archive/binary_oarchive.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "alignment.h"
#include "data_array.h"
#include "precomputation.h"
#include "suffix_array.h"
#include "time_util.h"
#include "translation_table.h"
#include "vocabulary.h"

namespace ar = boost::archive;
namespace fs = boost::filesystem;
namespace po = boost::program_options;
using namespace std;
using namespace extractor;

int main(int argc, char** argv) {
  po::options_description desc("Command line options");
  desc.add_options()
    ("help,h", "Show available options")
    ("source,f", po::value<string>(), "Source language corpus")
    ("target,e", po::value<string>(), "Target language corpus")
    ("bitext,b", po::value<string>(), "Parallel text (source ||| target)")
    ("alignment,a", po::value<string>()->required(), "Bitext word alignment")
    ("output,o", po::value<string>()->required(), "Output path")
    ("config,c", po::value<string>()->required(),
        "Path where the config file will be generated")
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
    ("min_frequency", po::value<int>()->default_value(1000),
        "Minimum number of occurrences for a pharse to be considered frequent");

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

  fs::path output_dir(vm["output"].as<string>());
  if (!fs::exists(output_dir)) {
    fs::create_directory(output_dir);
  }

  // Reading source and target data.
  Clock::time_point start_time = Clock::now();
  cerr << "Reading source and target data..." << endl;
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

  ofstream config_stream(vm["config"].as<string>());

  Clock::time_point start_write = Clock::now();
  string target_path = (output_dir / fs::path("target.bin")).string();
  config_stream << "target = " << target_path << endl;
  ofstream target_fstream(target_path);
  ar::binary_oarchive target_stream(target_fstream);
  target_stream << *target_data_array;
  Clock::time_point stop_write = Clock::now();
  double write_duration = GetDuration(start_write, stop_write);

  Clock::time_point stop_time = Clock::now();
  cerr << "Reading data took " << GetDuration(start_time, stop_time)
       << " seconds" << endl;

  // Constructing and compiling the suffix array.
  start_time = Clock::now();
  cerr << "Constructing source suffix array..." << endl;
  shared_ptr<SuffixArray> source_suffix_array =
      make_shared<SuffixArray>(source_data_array);

  start_write = Clock::now();
  string source_path = (output_dir / fs::path("source.bin")).string();
  config_stream << "source = " << source_path << endl;
  ofstream source_fstream(source_path);
  ar::binary_oarchive output_stream(source_fstream);
  output_stream << *source_suffix_array;
  stop_write = Clock::now();
  write_duration += GetDuration(start_write, stop_write);

  cerr << "Constructing suffix array took "
       << GetDuration(start_time, stop_time) << " seconds" << endl;

  // Reading alignment.
  start_time = Clock::now();
  cerr << "Reading alignment..." << endl;
  shared_ptr<Alignment> alignment =
      make_shared<Alignment>(vm["alignment"].as<string>());

  start_write = Clock::now();
  string alignment_path = (output_dir / fs::path("alignment.bin")).string();
  config_stream << "alignment = " << alignment_path << endl;
  ofstream alignment_fstream(alignment_path);
  ar::binary_oarchive alignment_stream(alignment_fstream);
  alignment_stream << *alignment;
  stop_write = Clock::now();
  write_duration += GetDuration(start_write, stop_write);

  stop_time = Clock::now();
  cerr << "Reading alignment took "
       << GetDuration(start_time, stop_time) << " seconds" << endl;

  shared_ptr<Vocabulary> vocabulary = make_shared<Vocabulary>();

  start_time = Clock::now();
  cerr << "Precomputing collocations..." << endl;
  Precomputation precomputation(
      vocabulary,
      source_suffix_array,
      vm["frequent"].as<int>(),
      vm["super_frequent"].as<int>(),
      vm["max_rule_span"].as<int>(),
      vm["max_rule_symbols"].as<int>(),
      vm["min_gap_size"].as<int>(),
      vm["max_phrase_len"].as<int>(),
      vm["min_frequency"].as<int>());

  start_write = Clock::now();
  string precomputation_path = (output_dir / fs::path("precomp.bin")).string();
  config_stream << "precomputation = " << precomputation_path << endl;
  ofstream precomp_fstream(precomputation_path);
  ar::binary_oarchive precomp_stream(precomp_fstream);
  precomp_stream << precomputation;

  string vocabulary_path = (output_dir / fs::path("vocab.bin")).string();
  config_stream << "vocabulary = " << vocabulary_path << endl;
  ofstream vocab_fstream(vocabulary_path);
  ar::binary_oarchive vocab_stream(vocab_fstream);
  vocab_stream << *vocabulary;
  stop_write = Clock::now();
  write_duration += GetDuration(start_write, stop_write);

  stop_time = Clock::now();
  cerr << "Precomputing collocations took "
       << GetDuration(start_time, stop_time) << " seconds" << endl;

  start_time = Clock::now();
  cerr << "Precomputing conditional probabilities..." << endl;
  TranslationTable table(source_data_array, target_data_array, alignment);

  start_write = Clock::now();
  string table_path = (output_dir / fs::path("bilex.bin")).string();
  config_stream << "ttable = " << table_path << endl;
  ofstream table_fstream(table_path);
  ar::binary_oarchive table_stream(table_fstream);
  table_stream << table;
  stop_write = Clock::now();
  write_duration += GetDuration(start_write, stop_write);

  stop_time = Clock::now();
  cerr << "Precomputing conditional probabilities took "
       << GetDuration(start_time, stop_time) << " seconds" << endl;

  cerr << "Total time spent writing: " << write_duration
       << " seconds" << endl;

  return 0;
}
