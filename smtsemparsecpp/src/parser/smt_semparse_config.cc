#ifndef SMT_SEMPARSE_CONFIG_CC
#define	SMT_SEMPARSE_CONFIG_CC

#include "smt_semparse_config.h"

#include <boost/algorithm/string.hpp>
#include <iostream>
#include <fstream>
#include <sstream>

using namespace std;

namespace al = boost::algorithm;

namespace smt_semparse {
  SMTSemparseConfig::SMTSemparseConfig(string settings_path, string dependencies_path, string experiment_dir, bool copy){
    if(copy){
      stringstream ss_copy_location_settings;
      ss_copy_location_settings << experiment_dir << "/settings.yaml";
      string copy_location_settings = ss_copy_location_settings.str();
      parse_file(settings_path, &copy_location_settings);
      stringstream ss_copy_location_dep;
      ss_copy_location_dep << experiment_dir << "/dependencies.yaml";
      string copy_location_dep = ss_copy_location_dep.str();
      parse_file(dependencies_path, &copy_location_dep);
    } else {
      parse_file(settings_path);
      parse_file(dependencies_path);
    }

    settings["experiment_dir"] = experiment_dir;

    stringstream data_dir_value;
    data_dir_value << detailed_at("smt_semparse") << "/data/";
    settings["data_dir"] = data_dir_value.str();

    if(detailed_at("np") == "true"){
      settings["train_name"] = "train.np";
    } else {
      settings["train_name"] = "train";
    }

    stringstream srilm_ngram_count_value;
    srilm_ngram_count_value << detailed_at("srilm") << "/bin/" << detailed_at("srilm_arch") << "/ngram-count";
    settings["srilm_ngram_count"] = srilm_ngram_count_value.str();

    stringstream moses_train_value;
    moses_train_value << detailed_at("moses") << "/scripts/training/train-model.perl";
    settings["moses_train"] = moses_train_value.str();

    /*for(auto it = settings.begin(); it != settings.end(); ++it){
      cerr << it->first << ": " << it->second << endl;
    }*/
  }

  map<string, string>& SMTSemparseConfig::get_settings(){
    return settings;
  }

  //if the ptr_exp_dir is not null the file is additionally written to that location
  void SMTSemparseConfig::parse_file(string path, string* ptr_copy_location){
    ifstream is_settings(path.c_str());
    if (!is_settings.is_open()){
      cerr << "The following file does not exist: " << path << endl;
      exit (EXIT_FAILURE);
    }
    string line;
    ofstream copy_outfile;
    if(ptr_copy_location != NULL){
      copy_outfile.open((*ptr_copy_location));
      if(!copy_outfile.is_open()){
        cerr << "The following file cannot be opened for writing" << (*ptr_copy_location) << endl;
        exit (EXIT_FAILURE);
      }
    }
    while(getline(is_settings, line)){
      istringstream is_line(line);
      if(ptr_copy_location != NULL){
        copy_outfile << line << endl;
      }
      string key;
      if(getline(is_line, key, ':')){
        string value;
        if(getline(is_line, value)){
          settings[key] = al::trim_copy(value.substr(0, value.find("#", 0)));
        }
      }
    }
    if(ptr_copy_location != NULL){
      copy_outfile.close();
    }
  }

  string SMTSemparseConfig::detailed_at(string key){
    try{
      return settings.at(key);
    } catch(std::out_of_range e){
      cerr << "The following key is missing: " << key << endl;
      exit (EXIT_FAILURE);
    }
  }


}  // namespace smt_semparse

#endif
