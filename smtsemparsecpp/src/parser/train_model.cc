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
#include "external_command.cc"

using namespace std;
using namespace smt_semparse;

// ./train_model -s settings.yaml -d dependencies.yaml -r work -y intersect

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
      ("symm,y", po::value<string>()->required(), "symmetry of alignment")
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

    // set up settings
    string original_working_directory = boost::filesystem::current_path().string();

    string exp_dir = vm["dir"].as<string>();
    SMTSemparseConfig config(vm["settings"].as<string>(), vm["dep"].as<string>(), exp_dir, true);

    // train LM
    cerr << "Train LM" << endl;
    string srilm_exec = config.detailed_at("srilm_ngram_count");
    stringstream ss_text;
    ss_text << config.detailed_at("experiment_dir") << "/train.mrl.lm";
    stringstream ss_lm;
    ss_lm << config.detailed_at("experiment_dir") << "/mrl.arpa";
    string input_lm = ss_text.str();
    const char *lm_argv[64] =
      {srilm_exec.c_str(), "-text", input_lm.c_str(),
      "-order", "5",
      "-no-sos",
      "-no-eos",
      "-lm",  ss_lm.str().c_str(),
      "-unk", NULL};
    if(0 != exec_prog(lm_argv)){
      cerr << "Something went wrong during LM creation..exiting" << endl;
      return 1;
    }

    //write cdec_tune.ini
    stringstream ss_cdec_tune_path;
    ss_cdec_tune_path << config.detailed_at("experiment_dir") << "/cdec_tune.ini";
    string cdec_tune_path = ss_cdec_tune_path.str();
    ofstream cdec_tune_out;
    cdec_tune_out.open(cdec_tune_path);
    if(!cdec_tune_out.is_open()){
      cerr << "The following file cannot be opened for writing" << cdec_tune_path << endl;
      return 1;
    }
    cdec_tune_out << "formalism=scfg" << endl
      << "intersection_strategy=cube_pruning" << endl
      << "cubepruning_pop_limit=200" << endl
      << "add_pass_through_rules=true" << endl
      << "scfg_max_span_limit=20" << endl
      << "feature_function=KLanguageModel " << config.detailed_at("experiment_dir") << "/mrl.arpa" << endl
      << "feature_function=WordPenalty" << endl
      << "density_prune=100" << endl;
      if(config.detailed_at("weights").find("mira") != std::string::npos){//TODO test
        cdec_tune_out << "feature_function=RuleIdentityFeatures" << endl
          << "feature_function=RuleSourceBigramFeatures" << endl
          << "feature_function=RuleTargetBigramFeatures" << endl
          << "feature_function=RuleShape" << endl;
      }
    cdec_tune_out.close();

    //write cdec_test.ini
    stringstream ss_cdec_test_path;
    ss_cdec_test_path << config.detailed_at("experiment_dir") << "/cdec_test.ini";
    string cdec_test_path = ss_cdec_test_path.str();
    ofstream cdec_test_out;
    cdec_test_out.open(cdec_test_path);
    if(!cdec_test_out.is_open()){
      cerr << "The following file cannot be opened for writing" << cdec_test_path << endl;
      return 1;
    }
    cdec_test_out << "formalism=scfg" << endl
      << "intersection_strategy=cube_pruning" << endl
      << "cubepruning_pop_limit=200" << endl
      << "add_pass_through_rules=true" << endl
      << "scfg_max_span_limit=20" << endl
      << "feature_function=KLanguageModel " << config.detailed_at("experiment_dir") << "/mrl.arpa" << endl
      << "feature_function=WordPenalty" << endl;
      if(config.detailed_at("weights").find("mira") != std::string::npos){//TODO test
        cdec_test_out << "feature_function=RuleIdentityFeatures" << endl
          << "feature_function=RuleSourceBigramFeatures" << endl
          << "feature_function=RuleTargetBigramFeatures" << endl
          << "feature_function=RuleShape" << endl;
      }
    cdec_test_out.close();

    //write weights.start
    stringstream ss_weights_start_path;
    ss_weights_start_path << config.detailed_at("experiment_dir") << "/weights.start";
    string weights_start_path = ss_weights_start_path.str();
    ofstream weights_start_out;
    weights_start_out.open(weights_start_path);
    if(!weights_start_out.is_open()){
      cerr << "The following file cannot be opened for writing" << weights_start_path << endl;
      return 1;
    }
    weights_start_out << "CountEF 0.1" << endl
      << "EgivenFCoherent -0.1" << endl
      << "Glue 0.01" << endl
      << "IsSingletonF -0.01" << endl
      << "IsSingletonFE -0.01" << endl
      << "LanguageModel 0.1" << endl
      << "LanguageModel_OOV -1" << endl
      << "MaxLexFgivenE -0.1" << endl
      << "MaxLexEgivenF -0.1" << endl
      << "PassThrough -0.1" << endl
      << "SampleCountF -0.1" << endl
      << "WordPenalty -0.1" << endl;

    weights_start_out.close();

    //write cdec_validate.ini
    stringstream ss_cdec_validate_path;
    ss_cdec_validate_path << config.detailed_at("experiment_dir") << "/cdec_validate.ini";
    string cdec_validate_path = ss_cdec_validate_path.str();
    ofstream cdec_validate_out;
    cdec_validate_out.open(cdec_validate_path);
    if(!cdec_validate_out.is_open()){
      cerr << "The following file cannot be opened for writing" << cdec_validate_path << endl;
      return 1;
    }
    cdec_validate_out << "formalism=scfg" << endl
      << "intersection_strategy=cube_pruning" << endl
      << "cubepruning_pop_limit=200" << endl
      << "scfg_max_span_limit=20" << endl;
    cdec_validate_out.close();

    // tune
    cerr << "Tune: ";
    string weights_output = "";
    stringstream ss_weights_path;
    ss_weights_path << config.detailed_at("workdir") << "/" << config.detailed_at("weights");
    string weights = ss_weights_path.str();
    if(config.detailed_at("weights") == "mert"){
      //need to cd to experiment_dir for mert
      cerr << "using mert" << endl;
      if(chdir(config.detailed_at("experiment_dir").c_str()) != 0){
        cerr << "Couldn't change directory for MERT" << endl;
        return 1;
      }
      //paste files
      stringstream ss_mert_paste;
      ss_mert_paste << config.detailed_at("cdec") << "/corpus/paste-files.pl";
      stringstream ss_tune_inline;
      ss_tune_inline << config.detailed_at("experiment_dir") << "/tune.inline.nl";
      string tune_inline= ss_tune_inline.str();
      stringstream ss_tune_mrl;
      ss_tune_mrl << config.detailed_at("experiment_dir") << "/tune.mrl";
      string tune_mrl= ss_tune_mrl.str();
      stringstream ss_tune_mert_file;
      ss_tune_mert_file << config.detailed_at("experiment_dir") << "/tune.mert";
      string tune_mert_file= ss_tune_mert_file.str();
      const char *mert_paste[64] =
            {ss_mert_paste.str().c_str(),
            tune_inline.c_str(),
            tune_mrl.c_str(),
            NULL};
      if(0 != exec_prog(mert_paste, &tune_mert_file)){
        cerr << "Something went wrong during the pasting of files for MERT..exiting" << endl;
        return 1;
      }

      //run mert
      stringstream ss_run_mert;
      ss_run_mert << config.detailed_at("cdec") << "/training/dpmert/dpmert.pl";
      const char *mert_args[64] =
            {ss_run_mert.str().c_str(),
            "-w", weights_start_path.c_str(),
            "-c", cdec_tune_path.c_str(),
            "-d", tune_mert_file.c_str(),
            NULL};
      if(0 != exec_prog(mert_args)){
        cerr << "Something went wrong during MERT..exiting" << endl;
        return 1;
      }

      if(chdir(original_working_directory.c_str()) != 0){
        cerr << "Couldn't revert to original directory after MERT" << endl;
        return 1;
      }
    } else if(config.detailed_at("weights") == "mira"){
      cerr << "using mira" << endl;
      if(chdir(config.detailed_at("experiment_dir").c_str()) != 0){
        cerr << "Couldn't change directory for MIRA" << endl;
        return 1;
      }
      stringstream ss_run_mira;
      ss_run_mira << config.detailed_at("cdec") << "/training/mira/kbest_mira";
      stringstream ss_tune_inline;
      ss_tune_inline << config.detailed_at("experiment_dir") << "/tune.inline.nl";
      string tune_inline= ss_tune_inline.str();
      stringstream ss_tune_mrl;
      ss_tune_mrl << config.detailed_at("experiment_dir") << "/tune.mrl";
      string tune_mrl= ss_tune_mrl.str();
      const char *mira_args[64] =
            {ss_run_mira.str().c_str(),
            "-w", weights_start_path.c_str(),
            "-c", cdec_tune_path.c_str(),
            "-i", tune_inline.c_str(),
            "-r", tune_mrl.c_str(),
            NULL};
      if(0 != exec_prog(mira_args)){
        cerr << "Something went wrong during MIRA..exiting" << endl;
        return 1;
      }

      if(chdir(original_working_directory.c_str()) != 0){
        cerr << "Couldn't revert to original directory after MIRA" << endl;
        return 1;
      }
    } else{
      cerr << "skipped. Copying weights: " << weights << endl;
      boost::filesystem::path src_path = weights;
      string filename = src_path.filename().string();
      stringstream ss_weights_output;
      ss_weights_output << config.detailed_at("experiment_dir") << "/" << filename;
      weights_output = ss_weights_output.str();
      ifstream is_weights(weights.c_str());
      if (!is_weights.is_open()){
        cerr << "The following file cannot be opened: " << weights << endl;
        return 1;
      }
      string line;
      ofstream copy_weights;
      copy_weights.open(weights_output);
      if(!copy_weights.is_open()){
        cerr << "The following file cannot be opened for writing" << weights_output << endl;
        return 1;
      }
      while(getline(is_weights, line)){
        copy_weights << line << endl;
      }
      copy_weights.close();
    }

    cerr << "Training complete" << endl;
    cerr << "-----------------" << endl;

  }
  catch(exception& e) {
    cerr << e.what() << endl;
    return 1;
  }

  return 0;
}
