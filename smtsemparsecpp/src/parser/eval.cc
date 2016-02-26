#ifndef EVAL_CC
#define	EVAL_CC

#include <cstdlib>
#include <iostream>
#include <fstream>
#include "boost/program_options.hpp"

using namespace std;

int main(int argc, char** argv) {

  try {
    namespace po = boost::program_options;
    po::options_description desc("Options");
    desc.add_options()
      ("help", "Print help messages")
      ("eval,e", po::value<string>()->required(), "file path where the eval results should be written")
      ("sigf,s", po::value<string>()->required(), "file path where the sigf results should be written")
      ("hyp,h", po::value<string>()->required(), "file path to the hypothesis' answers")
      ("gold,g", po::value<string>()->default_value(""), "file path to the gold answers");

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

    vector<string> gold_answers;
    int empty = 0;
    int fp = 0;
    int tp = 0;

    ifstream gold_file(vm["gold"].as<string>()); // /workspace/osm/smt-semparse/data/spoc/baseship.test.en
    if (!gold_file.is_open()){
      cerr << "The following file does not exist" << vm["gold"].as<string>() << endl;
      exit (EXIT_FAILURE);
    }
    string line;
    while(getline(gold_file, line)){
      gold_answers.push_back(line);
    }
    gold_file.close();

    ifstream hyp_file(vm["hyp"].as<string>()); // /workspace/osm/smt-semparse/data/spoc/baseship.test.en
    if (!hyp_file.is_open()){
      cerr << "The following file does not exist" << vm["hyp"].as<string>() << endl;
      exit (EXIT_FAILURE);
    }

    ofstream outfile_sigf;
    outfile_sigf.open(vm["sigf"].as<string>());
    if(!outfile_sigf.is_open()){
      cerr << "The following file cannot be opened for writing" << vm["eval"].as<string>() << endl;
      exit (EXIT_FAILURE);
    }

    int counter = 0;
    while(getline(hyp_file, line)){
      if(line == "empty" || line == ""){
        empty++;
        outfile_sigf << "0 0 1" << endl;
      } else if(line == gold_answers[counter]){
        tp++;
        outfile_sigf << "1 1 1" << endl;
      } else {
        fp++;
        outfile_sigf << "0 1 1" << endl;
      }
      counter++;
    }
    hyp_file.close();

    double precision = 0;
    double recall = 0;
    double f1 = 0;

    if((tp+fp)!=0){
      precision = 1.0 * tp / (tp + fp);
    }
    if(counter!=0){
      recall = 1.0 * tp /counter;
    }
    if((precision + recall)!=0){
      f1 = 2.0 * precision * recall / (precision + recall);
    }

    outfile_sigf.close();

    ofstream outfile_eval;
    outfile_eval.open(vm["eval"].as<string>());
    if(!outfile_eval.is_open()){
      cerr << "The following file cannot be opened for writing" << vm["eval"].as<string>() << endl;
      exit (EXIT_FAILURE);
    }
    outfile_eval << "p: " << precision << ", r: " << recall << ", f1: " << f1 << endl;
    outfile_eval.close();
  }
  catch(exception& e) {
    cerr << e.what() << endl;
    return 1;
  }

  return 0;
}


#endif
