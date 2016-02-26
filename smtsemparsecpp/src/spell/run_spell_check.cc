#ifndef RUN_SPELL_CHECK_CC
#define	RUN_SPELL_CHECK_CC

#include "spell_checker.cc"

#include <cstdlib>

#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>

int main(int argc, char** argv) {

  try {
    namespace po = boost::program_options;
    po::options_description desc("Options");
    desc.add_options()
      ("help", "Print help messages")
      ("stringcheck,s", po::value<string>()->required(), "The string to be checked");

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

    string check_this = vm["stringcheck"].as<string>();
    boost::replace_all(check_this, "?", "");
    boost::replace_all(check_this, "!", "");
    boost::replace_all(check_this, ".", "");
    boost::replace_all(check_this, ";", "");
    vector<string> words;
    boost::split(words, check_this, boost::is_any_of(" "));
    bool identical = 1;
    string spell_checked = correct_file(words);
    if(spell_checked != check_this){ identical = 0; }
    cout << identical << "{{splitter}}" << spell_checked << endl;

  }
  catch(exception& e) {
    cerr << e.what() << endl;
    return 1;
  }

  return 0;
}

#endif
