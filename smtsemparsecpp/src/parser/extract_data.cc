#include <cstdlib>
#include <iostream>
#include <fstream>
#include "boost/program_options.hpp"
#include <boost/filesystem.hpp>
#include <ctime>
#include <iomanip>
#include <unistd.h>

#include "smt_semparse_config.h"
#include "functionalizer.cc"
#include "extractor.cc"

using namespace std;
using namespace smt_semparse;

// ./train_model -s settings.yaml -d dependencies.yaml -r work

void run_test(string input, string mrl_out, string answer_out, NLParser& parser){

  ifstream nl_file(input);
  if (!nl_file.is_open()){
    cerr << "The following file could not be opened" << input << endl;
    exit (EXIT_FAILURE);
  }

  ofstream outfile_mrl;
  outfile_mrl.open(mrl_out);
  if(!outfile_mrl.is_open()){
    cerr << "The following file cannot be opened for writing" << mrl_out << endl;
    exit (EXIT_FAILURE);
  }

  ofstream outfile_answer;
  outfile_answer.open(answer_out);
  if(!outfile_answer.is_open()){
    cerr << "The following file cannot be opened for writing" << answer_out << endl;
    exit (EXIT_FAILURE);
  }

  string nl;
  struct smt_semparse::parseResult parse_result;
  while(getline(nl_file, nl)){
    parse_result = parser.parse_sentence(nl);
    outfile_answer << parse_result.answer << endl;
    if(outfile_mrl.is_open()){
      outfile_mrl << parse_result.mrl << endl;
    }
  }

  outfile_answer.close();
  outfile_mrl.close();
}

int main(int argc, char** argv) {

  try {
    namespace po = boost::program_options;
    po::options_description desc("Options");
    desc.add_options()
      ("help", "Print help messages")
      ("settings,s", po::value<string>()->required(), "settings path")
      ("dep,d", po::value<string>()->required(), "dependency path")
      ("dir,r", po::value<string>()->required(), "The directory where the model will be written");

    po::variables_map vm;
    try {
      po::store(po::parse_command_line(argc, argv, desc), vm);
      if(vm.count("help")) {
        cout << desc << endl;
        return 0;
      }
      po::notify(vm);
    }  catch(po::error& e) {
      cerr << "ERROR: " << e.what() << endl << endl;
      cerr << desc << endl;
      return 1;
    }

    string exp_dir = vm["dir"].as<string>();
    SMTSemparseConfig config(vm["settings"].as<string>(), vm["dep"].as<string>(), exp_dir, true);

    // extract data
    cerr << "Extracting data" << endl;
    extract_nlmaps(config);

  }
  catch(exception& e) {
    cerr << e.what() << endl;
    return 1;
  }

  return 0;
}
