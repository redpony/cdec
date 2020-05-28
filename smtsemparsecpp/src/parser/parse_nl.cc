#ifndef PARSE_NL_CC
#define	PARSE_NL_CC

#include "parse_nl.h"
#include "functionalizer.cc"
#include "ff_register.h"
#include "decoder.h"
#include "extractor.cc"

#include "interpret.cc"
#include "linearise.cc"
#include "nlmaps_query.h"

#include <boost/filesystem.hpp>
#include <iostream>
#include <istream>
#include <fstream>
#include <regex>


using namespace std;

namespace fs = boost::filesystem;
using namespace extractor; //required to use Clock directly

namespace smt_semparse {

  NLParser::NLParser(string &exp_dir, string &db_dir, SMTSemparseConfig &config_obj, string &daemon_adress):
      experiment_dir(exp_dir), database_dir(db_dir),
      config(config_obj),
      requester(daemon_adress.c_str(), 100000){
      register_feature_functions();
  }

  NLParser::NLParser(string &exp_dir, string &db_dir, string &daemon_adress): experiment_dir(exp_dir), database_dir(db_dir),
      config(experiment_dir + "/settings.yaml", experiment_dir + "/dependencies.yaml", experiment_dir),
      requester(daemon_adress.c_str(), 100000){
      register_feature_functions();
  }

  parseResult& NLParser::parse_sentence(string &sentence, NominatimCheck* nom, string preset_tmp_dir, bool geojson, bool nom_lookup){
    //default result
    parse_result.mrl = "";
    parse_result.answer = "";

    //preprocess_sentence
    ps = preprocess_nl(sentence, config, true, nom);

    //create tmp dir
    string fs_tmp_dir_path = "";
    if(preset_tmp_dir!=""){
      fs_tmp_dir_path = preset_tmp_dir;
    } else {
      stringstream tmp_dir_location;
      tmp_dir_location << "/tmp/parse_nl_cpp_" << fs::unique_path().string();
      fs_tmp_dir_path = tmp_dir_location.str();
    }
    fs::path fs_tmp_dir(fs_tmp_dir_path);
    if (!fs::is_directory(fs_tmp_dir)) {
      fs::create_directory(fs_tmp_dir);
    }

    //get grammar
    stringstream ss_message_plus_path;
    if(preset_tmp_dir==""){ //for testing puproses we wont send a tmp_dir to the daemon, thus it should be written into the grammar folder that was registered when the daemon was started and it can be inspected
      ss_message_plus_path << ps.sentence;
    } else {
      ss_message_plus_path << ps.sentence << " <|||> " << fs_tmp_dir_path;
    }
    string message_plus_path = ss_message_plus_path.str();
    string sentence_to_decode = "";
    sentence_to_decode = requester.request_for_sentence(message_plus_path.c_str());
    if(sentence_to_decode == ""){
      cerr << "Warning: Extractor daemon did not return anything for this sentence: "
      << ps.sentence << endl;
    }

    //decode
    string weights_file_name = "";
    if(config.detailed_at("weights")=="mira"){
        weights_file_name = "weights.mira-final.gz";
    } else if(config.detailed_at("weights")=="mert"){
        weights_file_name = "dpmert/weights.final";
    } else{
        fs::path weights_path(config.detailed_at("weights"));
        weights_file_name = weights_path.filename().string();
    }

    stringstream ss_config;
    ss_config << "formalism=scfg" << endl;
    ss_config << "intersection_strategy=cube_pruning" << endl;
    ss_config << "cubepruning_pop_limit=1000" << endl;
    ss_config << "scfg_max_span_limit=20" << endl;
    ss_config << "feature_function=KLanguageModel " << experiment_dir << "/mrl.arpa" << endl;
    ss_config << "feature_function=WordPenalty" << endl;
    ss_config << "weights=" << experiment_dir << "/" << weights_file_name << endl;
    ss_config << "k_best=" << config.detailed_at("nbest") << endl;
    ss_config << "unique_k_best=" << endl;
    ss_config << "add_pass_through_rules=" << endl;
    istringstream config_file(ss_config.str());
    Decoder decoder(&config_file);
    vector<string> kbest_out;
    cerr << "sentence_to_decode: " << sentence_to_decode << endl;
    decoder.Decode(sentence_to_decode, NULL, &kbest_out);


    /*int count = 0;
    for(vector<string>::iterator it2(kbest_out.begin()); it2 != kbest_out.end(); ++it2, ++count){
      cerr << count<<": " << *it2 << endl;
    }*/


    if(kbest_out.size()>0){
      //convert structure
      functionalize_kbest(kbest_out, config, ps, parse_result); //fills parse_result.recover_query & mrl

      if(parse_result.mrl!=""){
        NLmaps_query query_info;
        if(nom_lookup){
          boost::replace_all(parse_result.mrl, "{nominatim:Frankenthal}", "1705457707");
        }
        query_info.mrl = parse_result.mrl;
        preprocess_mrl(&query_info);

        Evaluator mrl_eval(database_dir);
        int init_return = -1;
        if(geojson){
          init_return = mrl_eval.initalise(&query_info, true);
        } else {
          init_return = mrl_eval.initalise(&query_info, false);
        }
        if(init_return != 0){
          cerr << "Warning: Failed to initialise the following query with error code " << init_return << ": " << query_info.mrl << endl;
        }
        parse_result.answer = mrl_eval.interpret(&query_info);
        //always get latlong
        stringstream ss_latlong;
        stringstream ss_pre_latlong;
        double center_lat = 0.0;
        double center_lon = 0.0;
        bool first = true;
        if(geojson){
          ss_latlong << "\"features\": [";
          for(auto it = query_info.elements.begin(); it != query_info.elements.end(); it++){
            center_lat += it->lat_lon.first;
            center_lon += it->lat_lon.second;
            if(first){
              first = false;
            } else {
              ss_latlong << ",";
            }
            //lat and lon need to be revered for geojson
            if(it->name==""){ it->name = "<i>Unnamed</i>"; }
            ss_latlong << "{\"type\": \"Feature\",\"properties\": {\"popupContent\": \"<b>"<<it->name<<"</b>";
            if(it->value != it->name && it->value!=""){ ss_latlong<<"<br/><b>lat</b> "<<it->lat_lon.first<<" <b>lon</b> " <<it->lat_lon.second; }
            if(it->value != it->name && it->value!=""){ ss_latlong<<"<br/>"<<it->value; }
            if(it->street_number != "" || it->street != ""){ ss_latlong<<"<br/>"<<it->street_number<<" "<<it->street; }
            if(it->postcode != "" || it->town != ""){ ss_latlong<<"<br/>"<<it->postcode<<" "<<it->town; }

            ss_latlong << "\"},\"geometry\": {\"type\": \"Point\",\"coordinates\": ["
              <<fixed<<setprecision(7)<<it->lat_lon.second<<", "<<fixed<<setprecision(7)<<it->lat_lon.first<<"]}}";
          }
          center_lat = center_lat / query_info.elements.size();
          center_lon = center_lon / query_info.elements.size();
          ss_latlong << "]";
          //we note latlong here because we don't want the gps coordinates to be printed in the answer box
          bool latlon = 0;
          if(query_info.latlon){
            latlon = 1;
          }
          ss_pre_latlong << "{\"correctly_empty\": \""<<latlon<<"\",\"lat\": \""<<center_lat<<"\",\"lon\": \""<<center_lon<<"\"," << ss_latlong.str() << "}";
        } else {
          for(auto it = query_info.elements.begin(); it != query_info.elements.end(); it++){
            if(first){
              first = false;
              ss_latlong << fixed<<setprecision(7)<<it->lat_lon.first << " " << fixed<<setprecision(7)<<it->lat_lon.second;
            } else {
              ss_latlong << ", " << fixed<<setprecision(7)<<it->lat_lon.first << " " << fixed<<setprecision(7)<<it->lat_lon.second;
            }
          }
        }
        parse_result.latlong = ss_pre_latlong.str();
      } else {
        parse_result.mrl = "no mrl found";
        parse_result.answer = "empty";
      }
    }

    //delete tmp dir
    if(preset_tmp_dir==""){ //if the tmp dir path was set, then we assume the calling process takes care of it
      fs::remove_all(fs_tmp_dir);
    }

    return parse_result;
  }

}  // namespace smt_semparse

#endif
