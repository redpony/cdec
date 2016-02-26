#ifndef RUN_SPELL_CHECK_CC
#define	RUN_SPELL_CHECK_CC

#include "nominatim_check.cc"
#include "../parser/extractor.cc"

#include <cstdlib>

#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>

// bin/run_nominatim_check -n "ipc:///tmp/sa_daemon_test.ipc" -s "How many McDonald's are there in Frankenthal ?"
int main(int argc, char** argv) {

  try {
    namespace po = boost::program_options;
    po::options_description desc("Options");
    desc.add_options()
      ("help", "Print help messages")
      ("stringcheck,s", po::value<string>()->required(), "The string to be checked")
      ("nominatim_check_adress,n", po::value<string>()->required(), "Suffix array daemon's address");

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

    NominatimCheck nom(vm["nominatim_check_adress"].as<string>().c_str());
    string nl = vm["stringcheck"].as<string>();
    smt_semparse::tokenise(nl);
    nom.protect_sentence_for_nominatim(&nl);
    cout << nl << endl;
  }
  catch(exception& e) {
    cerr << e.what() << endl;
    return 1;
  }

  return 0;
}

#endif
