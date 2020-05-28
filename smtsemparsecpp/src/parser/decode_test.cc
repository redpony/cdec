#ifndef PARSE_NL_FILE_CC
#define	PARSE_NL_FILE_CC

#include "parse_nl.h"
#include "smt_semparse_config.h"
#include "functionalizer.cc"
#include "ff_register.h"
#include "decoder.h"

#include "interpret.cc"
#include "linearise.cc"
#include "nlmaps_query.h"

#include <cstdlib>
#include <iostream>
#include <fstream>
#include "boost/program_options.hpp"
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>

using namespace std;

namespace fs = boost::filesystem;


//test file needs to be preprocessed & grammar extracted
//assumes a preprocessed test file test.nl, an non_stemmed test file test.nostem.nl and
//a file with grammar links test.inline.nl
/*./decode_test \
  -d /workspace/osm/overpass/db/ \
  -m /workspace/osm/cdec/smtsemparsecpp/work/2015-11-10T19.44.12 \
  -p hyp.question \
  -a hyp.answers \
  -o hyp.latlong \
  -l hyp.mrls*/
// ./decode_test -d /workspace/osm/overpass/db/ -m /workspace/osm/cdec/smtsemparsecpp/work// -a answers -l mrls -p question -o latlong
int main(int argc, char** argv) {

  try {
    namespace po = boost::program_options;
    po::options_description desc("Options");
    desc.add_options()
      ("help", "Print help messages")
      ("db_dir,d", po::value<string>()->required(), "(Full) database directory path")
      ("model,m", po::value<string>()->required(), "(Full) parser model directory path")
      ("print,p", po::value<string>()->default_value(""), "Query file's output path")
      ("answer,a", po::value<string>()->required(), "Answer file's output path")
      ("latlong,o", po::value<string>()->default_value(""), "Latlong file's output path")
      ("mrl,l", po::value<string>()->default_value(""), "mrl file's output path")
      ("settings,s", po::value<string>()->default_value(""), "settings path");

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

    string nl;
    string db_dir = vm["db_dir"].as<string>(); // /workspace/osm/overpass/db
    string model = vm["model"].as<string>(); // /workspace/osm/smt-semparse/work/cdec_train_test/intersect_stem_mert_@_pp.l.w
    vector<string> stemmed_lines;
    vector<string> nonstemmed_lines;
    vector<string> grammar_lines;

    ifstream nl_file_stemmed(model+"/test.nl");
    if (!nl_file_stemmed.is_open()){
      cerr << "The following file does not exist: test.nl in " << model << endl;
      exit (EXIT_FAILURE);
    }
    while(getline(nl_file_stemmed, nl)){
      stemmed_lines.push_back(nl);
    }
    nl_file_stemmed.close();

    ifstream nl_file_nonstemmed(model+"/test.nostem.nl");
    if (!nl_file_nonstemmed.is_open()){
      cerr << "The following file does not exist: test.nostem.nl in " << model << endl;
      exit (EXIT_FAILURE);
    }
    while(getline(nl_file_nonstemmed, nl)){
      nonstemmed_lines.push_back(nl);
    }
    nl_file_nonstemmed.close();

    ifstream nl_file_grammar(model+"/test.inline.nl");
    if (!nl_file_grammar.is_open()){
      cerr << "The following file does not exist: test.inline.nl in " << model << endl;
      exit (EXIT_FAILURE);
    }
    while(getline(nl_file_grammar, nl)){
      grammar_lines.push_back(nl);
    }
    nl_file_grammar.close();

    if(stemmed_lines.size()!=nonstemmed_lines.size() || stemmed_lines.size()!=grammar_lines.size()){
      cerr << "The files test.nl, test.nostem.nl and test.inline.nl need to be of equal length"<< endl;
      exit (EXIT_FAILURE);
    }

    ofstream outfile_answer;
    outfile_answer.open(vm["answer"].as<string>());
    if(!outfile_answer.is_open()){
      cerr << "The following file cannot be opened for writing" << vm["answer"].as<string>() << endl;
      exit (EXIT_FAILURE);
    }

    ofstream outfile_mrl;
    if(vm["mrl"].as<string>() != ""){
      outfile_mrl.open(vm["mrl"].as<string>());
      if(!outfile_mrl.is_open()){
        cerr << "The following file cannot be opened for writing" << vm["mrl"].as<string>() << endl;
        exit (EXIT_FAILURE);
      }
    }

    ofstream outfile_latlong;
    if(vm["latlong"].as<string>() != ""){
      outfile_latlong.open(vm["latlong"].as<string>());
      if(!outfile_latlong.is_open()){
        cerr << "The following file cannot be opened for writing" << vm["latlong"].as<string>() << endl;
        exit (EXIT_FAILURE);
      }
    }

    ofstream outfile_print;
    if(vm["print"].as<string>() != ""){
      outfile_print.open(vm["print"].as<string>());
      if(!outfile_print.is_open()){
        cerr << "The following file cannot be opened for writing" << vm["print"].as<string>() << endl;
        exit (EXIT_FAILURE);
      }
    }

    //load prerequisites
    register_feature_functions();
    smt_semparse::SMTSemparseConfig* config = NULL;
    if(vm["settings"].as<string>()!=""){
      config = new smt_semparse::SMTSemparseConfig(vm["settings"].as<string>(), model + "/dependencies.yaml", model, false);
    } else {
      config = new smt_semparse::SMTSemparseConfig(model + "/settings.yaml", model + "/dependencies.yaml", model, false);
    }

    int c = 0;
		Evaluator mrl_eval(db_dir);
    for(vector<string>::iterator it(grammar_lines.begin()); it != grammar_lines.end(); ++it, ++c){
      struct smt_semparse::parseResult parse_result;
      struct smt_semparse::preprocessed_sentence ps;
      //default result
      parse_result.mrl = "";
      parse_result.answer = "";

      //preprocessed sentence
      ps.non_stemmed = nonstemmed_lines[c];
      ps.sentence = stemmed_lines[c];

      //decode
      string weights_file_name = "";
      if(config->detailed_at("weights")=="mira"){
          weights_file_name = "weights.mira-final.gz";
      } else if(config->detailed_at("weights")=="mert"){
          weights_file_name = "dpmert/weights.final";
      } else{
          fs::path weights_path(config->detailed_at("weights"));
          weights_file_name = weights_path.filename().string();
      }

      stringstream ss_config;
      ss_config << "formalism=scfg" << endl;
      ss_config << "intersection_strategy=cube_pruning" << endl;
      ss_config << "cubepruning_pop_limit=1000" << endl;
      ss_config << "scfg_max_span_limit=20" << endl;
      ss_config << "feature_function=KLanguageModel " << model << "/mrl.arpa" << endl;
      ss_config << "feature_function=WordPenalty" << endl;
      ss_config << "weights=" << model << "/" << weights_file_name << endl;
      ss_config << "k_best=" << config->detailed_at("nbest") << endl;
      ss_config << "unique_k_best=" << endl;
      ss_config << "add_pass_through_rules=" << endl;
      if(boost::contains(config->detailed_at("weights"), "mira")){
        ss_config << "feature_function=RuleIdentityFeatures" << endl;
        ss_config << "feature_function=RuleSourceBigramFeatures" << endl;
        ss_config << "feature_function=RuleTargetBigramFeatures" << endl;
        ss_config << "feature_function=RuleShape" << endl;
      }
      istringstream config_file(ss_config.str());
      Decoder decoder(&config_file);
      vector<string> kbest_out;
      decoder.Decode(grammar_lines[c], NULL, &kbest_out);

      if(kbest_out.size()>0){
        //convert structure
        functionalize_kbest(kbest_out, (*config), ps, parse_result); //fills parse_result.recover_query & mrl

        if(parse_result.mrl!=""){
          //obtain answer
					NLmaps_query query_info;
					query_info.mrl = parse_result.mrl;
					preprocess_mrl(&query_info);
					int init_return = mrl_eval.initalise(&query_info);
					if(init_return != 0){
						cerr << "Warning: Failed to initialise the following query with error code " << init_return << ": " << query_info.mrl << endl;
					}
					parse_result.answer = mrl_eval.interpret(&query_info);
        } else {
          parse_result.mrl = "no mrl found";
          parse_result.answer = "empty";
        }
      }


      if(outfile_print.is_open()){
        outfile_print << parse_result.recover_fun << endl;
      }
      outfile_answer << parse_result.answer << endl;
      if(outfile_mrl.is_open()){
        outfile_mrl << parse_result.mrl << endl;
      }
      if(outfile_latlong.is_open()){
        outfile_latlong << parse_result.latlong << endl;
      }
    }

    delete config;

    outfile_answer.close();
    if(outfile_mrl.is_open()){
      outfile_mrl.close();
    }
    if(outfile_print.is_open()){
      outfile_print.close();
    }
    if(outfile_latlong.is_open()){
      outfile_latlong.close();
    }
  }
  catch(exception& e) {
    cerr << e.what() << endl;
    return 1;
  }

  return 0;
}

#endif
