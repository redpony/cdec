#ifndef PARSE_NL_FILE_CC
#define	PARSE_NL_FILE_CC

#include "parse_nl.h"
#include "smt_semparse_config.h"
#include "../name_lexicon/nominatim_check.h"

#include <cstdlib>
#include <iostream>
#include <fstream>

#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>

using namespace std;

/*./parse_nl_file \
  -r ipc:///tmp/extract_daemon_2015-11-10T19.44.12.ipc \
  -d /workspace/osm/overpass/db/ \
  -m /workspace/osm/cdec/smtsemparsecpp/work/2015-11-10T19.44.12 \
  -f me \
  -p hyp.question \
  -a hyp.answers \
  -o hyp.latlong \
  -l hyp.mrls*/
// /workspace/osm/cdec/extractor/extract_daemon -g grammar_daemon/ -c extract.ini -a "ipc:///tmp/extract_daemon_vali.ipc" --tight_phrases 0
// bin/parse_nl_file -r "ipc:///tmp/extract_daemon_vali.ipc" -d /workspace/osm/overpass/db/ -m /workspace/osm/cdec/smtsemparsecpp/models/mytok_en/ -a answers -l mrls -p question -o latlong -g -f this
// /workspace/osm/cdec/extractor/extract_daemon -g grammar_daemon/ -c extract.ini -a "ipc:///tmp/extract_daemon_en.ipc" --tight_phrases 0
// bin/parse_nl_file -r "ipc:///tmp/extract_daemon_en.ipc" -d /workspace/osm/overpass/db/ -m /workspace/osm/cdec/smtsemparsecpp/models/mytok_en/ -a answers -l mrls -p question -o latlong -g -f this
//nom request: bin/parse_nl_file -r "ipc:///tmp/extract_daemon_vali.ipc" -d /workspace/osm/overpass/db/ -m /workspace/osm/cdec/smtsemparsecpp/models/mytok_en/ -a answers -l mrls -p question -o latlong -g -n "ipc:///tmp/sa_daemon_test.ipc" -f 3
//nom true: bin/parse_nl_file -r "ipc:///tmp/extract_daemon_vali.ipc" -d /workspace/osm/overpass/db/ -m /workspace/osm/cdec/smtsemparsecpp/models/mytok_en/ -a answers -l mrls -p question -o latlong -g -n "true" -f this
int main(int argc, char** argv) {

  try {
    namespace po = boost::program_options;
    po::options_description desc("Options");
    desc.add_options()
      ("help", "Print help messages")
      ("file,f", po::value<string>()->required(), "Query file's input path")
      ("db_dir,d", po::value<string>()->required(), "(Full) database directory path")
      ("daemon_adress,r", po::value<string>()->required(), "Grammar daemon's address")
      ("nominatim_check,n", po::value<string>()->default_value(""), "Either the adress for the nominatim checker, or if that has been done already, pass true to have it evaluated against nominatim before query execution")
      ("model,m", po::value<string>()->required(), "(Full) parser model directory path")
      ("print,p", po::value<string>()->default_value(""), "Query file's output path")
      ("answer,a", po::value<string>()->required(), "Answer file's output path")
      ("latlong,o", po::value<string>()->default_value(""), "Latlong file's output path")
      ("mrl,l", po::value<string>()->default_value(""), "mrl file's output path")
      ("geojson,g", po::bool_switch()->default_value(false), "output in latlong file should be in geojson format")
      ("settings,s", po::value<string>()->default_value(""), "settings path")
      ("tmpdir,t", po::value<string>()->default_value(""), "use this (tmp) directory path to write intermediate files to");

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

    string db_dir = vm["db_dir"].as<string>(); // /workspace/osm/overpass/db
    string model = vm["model"].as<string>(); // /workspace/osm/smt-semparse/work/cdec_train_test/intersect_stem_mert_@_pp.l.w
    ifstream nl_file(vm["file"].as<string>()); // /workspace/osm/smt-semparse/data/spoc/baseship.test.en

    if (!nl_file.is_open()){
      cerr << "The following file does not exist" << vm["file"].as<string>() << endl;
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

    string daemon_adress = vm["daemon_adress"].as<string>();
    smt_semparse::NLParser* parser = NULL;
    if(vm["settings"].as<string>()!=""){
      smt_semparse::SMTSemparseConfig config(vm["settings"].as<string>(), model + "/dependencies.yaml", model, false);
      parser = new smt_semparse::NLParser(model, db_dir, config, daemon_adress);
    } else {
      parser = new smt_semparse::NLParser(model, db_dir, daemon_adress);
    }
    string nl;
    string tmp_dir = vm["tmpdir"].as<string>();

    NominatimCheck* ptr_nom = NULL;
    bool nom_lookup = false;
    if(vm["nominatim_check"].as<string>()!=""){
      if(vm["nominatim_check"].as<string>()!="true"){
        NominatimCheck nom(vm["nominatim_check"].as<string>().c_str());
        ptr_nom = &nom;
      }
      nom_lookup = true;
    }

    while(getline(nl_file, nl)){
      struct smt_semparse::parseResult parse_result = parser->parse_sentence(nl, ptr_nom, tmp_dir, vm["geojson"].as<bool>(), nom_lookup);
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

    delete parser;

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
