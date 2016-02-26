#ifndef EXTRACTOR_CC
#define	EXTRACTOR_CC

#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>
#include <boost/format.hpp>
#include <sstream>
#include <map>
#include <assert.h>
#include <fstream>

#include "porter2_stemmer.h"
#include "smt_semparse_config.h"
#include "../name_lexicon/nominatim_check.h"
#include "parse_nl.h"

using namespace std;

namespace smt_semparse {

  static bool check_ending(string const &full_string, string const &ending) {
    if (full_string.length() >= ending.length()) {
        return (0 == full_string.compare(full_string.length() - ending.length(), ending.length(), ending));
    } else {
        return false;
    }
  }

  static int count_arguments(string s){
    bool args = false;
    int parens = 0;
    int commas = 0 ;
    int i = 0;
    //while parens >= 0 and i < len(s):
    while(i < s.length() && ((!(args) && parens == 0) || (args && parens > 0))){
      const char& c = s.at(i);
      if(c == '('){
        args = true;
        parens += 1;
      } else if(c == ')'){
        parens -= 1;
      } else if(parens == 1 && c == ','){
        commas += 1;
      } else if(parens < 1 && c == ','){
        break;
      }
      i += 1;
    }
    if(args){
      return commas + 1;
    }
    assert(commas==0);
    return 0;
  }

  static string after_nth(string mrl, string& token, int n){
    while(n > 0){
      try{
        boost::smatch sm;
        stringstream ss_pattern;
        ss_pattern << "\\b";
        ss_pattern << boost::regex_replace(token, boost::regex("[.^$|()\\[\\]{}*+?\\\\]"), "\\\\$&", boost::match_default | boost::format_perl);
        ss_pattern << "\\b";
        regex_search(mrl, sm, boost::regex(ss_pattern.str()));
        mrl = mrl.substr(sm.position() + sm.length());
        n = n - 1;
      } catch(...){
        cerr << "Warning: error on token " << token << " in mrl " << mrl << endl;
        return "";
      }
    }
    return mrl;
  }

  static string preprocess_mrl(string mrl, bool no_arity){
    stringstream ss_lin;
    boost::trim(mrl);

    //sequence of characters that does not contain ( or ) : [^\\(\\)]
    mrl = boost::regex_replace(mrl, boost::regex(",' *([^\\(\\)]*?)\\((.*?) *'\\)"),",'$1BRACKETOPEN$2')"); //need to protect brackets that occur in values, assumes that there is at most one open ( and 1 close)
    mrl = boost::regex_replace(mrl, boost::regex(",' *([^\\(\\)]*?)\\)([^\\(\\)]*?) *'\\)"),",'$1BRACKETCLOSE$2')");
    boost::replace_all(mrl, " ", "€");
    mrl = boost::regex_replace(mrl, boost::regex("(?<=([^,\\(\\)]))'(?=([^,\\(\\)]))"), "SAVEAPO");
    mrl = boost::regex_replace(mrl, boost::regex("and\\(' *([^\\(\\)]+?) *',' *([^\\(\\)]+?) *'\\)"), "and($1ARITY_SEPARITY_STR','$2ARITY_SEPARITY_STR)"); //for when a and() surrounds two end values
    mrl = boost::regex_replace(mrl, boost::regex("\\(' *([^\\(\\)]+?) *'\\)"), "($1ARITY_SEPARITY_STR)"); //a bracket ( or ) is not allowed withing any key or value
    mrl = boost::regex_replace(mrl, boost::regex("([,\\)\\(])or\\(([^\\(\\)]+?)','([^\\(\\)]+?)ARITY_SEPARITY_STR\\)"), "$1or($2ARITY_SEPARITY_STR','$3ARITY_SEPARITY_STR)"); //for when a or() surrounds two values
    //mrl = boost::regex_replace(mrl, boost::regex("' *(.+?) *'"), "$1ARITY_SEPARITY_STR");  //uncomment for keyval versions and comment the above
    boost::replace_all(mrl, "ARITY_SEP", ARITY_SEP);
    boost::replace_all(mrl, "ARITY_STR", ARITY_STR);
    mrl = boost::regex_replace(mrl, boost::regex("\\s+"), " ");
    mrl = boost::regex_replace(mrl, boost::regex("'"), ""); //comment for keyval versions
    string mrl_noparens = mrl;
    boost::replace_all(mrl_noparens, "(", " ");
    boost::replace_all(mrl_noparens, ")", " ");
    mrl_noparens = boost::regex_replace(mrl_noparens, boost::regex("\\s+"), " ");
    string mrl_nocommas = mrl_noparens;
    boost::replace_all(mrl_nocommas, ",", " ");
    mrl_nocommas = boost::regex_replace(mrl_nocommas, boost::regex("\\s+"), " ");

    map<string, int> seen;
    vector<string> elements;
    boost::trim(mrl_nocommas);
    boost::split(elements, mrl_nocommas, boost::is_any_of(" "));
    for(vector<string>::iterator it = elements.begin(); it != elements.end(); ++it) {
      if (seen.find(*it) == seen.end()){
        seen[*it] = 1;
      } else {
        seen[*it]++;
      }
      if(check_ending(*it, "@s")){
        if(no_arity){
          ss_lin << (*it).substr(0, (*it).length()-2) << " ";
        } else {
          ss_lin << *it << " ";
        }
        continue;
      }
      int args = count_arguments(after_nth(mrl, *it, seen[*it]));
      if(no_arity){
        ss_lin << *it << " ";
      } else {
        ss_lin << *it << ARITY_SEP << args << " ";
      }
    }

    string lin = ss_lin.str();
    boost::trim(lin);
    return lin;
  }

  static void tokenise(string& nl){
    if(check_ending(nl, " .") || check_ending(nl, " ?") || check_ending(nl, " !")){
      nl = nl.substr(0, nl.length()-2);
    }
    if(check_ending(nl, ".") || check_ending(nl, "?") || check_ending(nl, "!")){
      nl = nl.substr(0, nl.length()-1);
    }
  }

  static preprocessed_sentence preprocess_nl(string nl, SMTSemparseConfig& config, bool skip = false, NominatimCheck* nom = NULL){
    preprocessed_sentence ps;
    boost::trim(nl);
    ps.non_stemmed = nl;
    ps.stemmed = nl;

    if(!skip){
      boost::to_lower(ps.non_stemmed);
      boost::replace_all(ps.non_stemmed, "@", "xxatxx");
    }

    boost::to_lower(ps.stemmed);
    boost::replace_all(ps.stemmed, "@", "xxatxx");

    tokenise(ps.non_stemmed);
    tokenise(ps.stemmed);

    //stem
    vector<string> words_to_stem;
    stringstream ss;
    boost::split(words_to_stem, ps.stemmed, boost::is_any_of(" "));
    for(vector<string>::iterator it = words_to_stem.begin(); it != words_to_stem.end(); ++it) {
      Porter2Stemmer::stem(*it);
      ss << *it << " ";
    }
    ps.stemmed = ss.str();
    boost::trim(ps.stemmed);

    //nominatim Check
    if(nom!=NULL){
      nom->protect_sentence_for_nominatim(&ps.non_stemmed, &ps.stemmed);
    }

    if(config.detailed_at("stem")=="true"){
      ps.sentence = ps.stemmed;
    } else {
      ps.sentence = ps.non_stemmed;
    }

    return ps;
  }

  static void extract_nlmaps(SMTSemparseConfig& config){
    string exp_dir = config.detailed_at("experiment_dir");
    // open files for writing
    ofstream out_train_nl;
    stringstream ss_out_train_nl;
    ss_out_train_nl << exp_dir << "/train.nl";
    out_train_nl.open(ss_out_train_nl.str());

    ofstream out_train_mrl;
    stringstream ss_out_train_mrl;
    ss_out_train_mrl << exp_dir << "/train.mrl";
    out_train_mrl.open(ss_out_train_mrl.str());

    ofstream out_train_mrl_lm;
    stringstream ss_out_train_mrl_lm;
    ss_out_train_mrl_lm << exp_dir << "/train.mrl.lm";
    out_train_mrl_lm.open(ss_out_train_mrl_lm.str());

    ofstream out_train_ori;
    stringstream ss_out_train_ori;
    ss_out_train_ori << exp_dir << "/train.ori";
    out_train_ori.open(ss_out_train_ori.str());

    ofstream out_train_both;
    stringstream ss_out_train_both;
    ss_out_train_both << exp_dir << "/train.both";
    out_train_both.open(ss_out_train_both.str());

    ofstream out_tune_nl;
    stringstream ss_out_tune_nl;
    ss_out_tune_nl << exp_dir << "/tune.nl";
    out_tune_nl.open(ss_out_tune_nl.str());

    ofstream out_tune_mrl;
    stringstream ss_out_tune_mrl;
    ss_out_tune_mrl << exp_dir << "/tune.mrl";
    out_tune_mrl.open(ss_out_tune_mrl.str());

    ofstream out_tune_gold;
    stringstream ss_out_tune_gold;
    ss_out_tune_gold << exp_dir << "/tune.gold";
    out_tune_gold.open(ss_out_tune_gold.str());

    ofstream out_test_nl;
    stringstream ss_out_test_nl;
    ss_out_test_nl << exp_dir << "/test.nl";
    out_test_nl.open(ss_out_test_nl.str());

    ofstream out_test_nl_nostem;
    stringstream ss_out_test_nl_nostem;
    ss_out_test_nl_nostem << exp_dir << "/test.nostem.nl";
    out_test_nl_nostem.open(ss_out_test_nl_nostem.str());

    ofstream out_test_mrl;
    stringstream ss_out_test_mrl;
    ss_out_test_mrl << exp_dir << "/test.mrl";
    out_test_mrl.open(ss_out_test_mrl.str());

    ofstream out_test_ori;
    stringstream ss_out_test_ori;
    ss_out_test_ori << exp_dir << "/test.ori";
    out_test_ori.open(ss_out_test_ori.str());

    ofstream out_test_gold;
    stringstream ss_out_test_gold;
    ss_out_test_gold << exp_dir << "/test.gold";
    out_test_gold.open(ss_out_test_gold.str());

    ofstream out_test_neg_nl;
    ofstream out_test_neg_nl_nostem;
    ofstream out_test_neg_mrl;
    ofstream out_test_neg_ori;
    ofstream out_test_neg_gold;
    if(config.detailed_at("neg")!=""){
      stringstream ss_out_test_neg_nl;
      ss_out_test_neg_nl << exp_dir << "/test_neg.nl";
      out_test_neg_nl.open(ss_out_test_neg_nl.str());

      stringstream ss_out_test_neg_nl_nostem;
      ss_out_test_neg_nl_nostem << exp_dir << "/test_neg.nostem.nl";
      out_test_neg_nl_nostem.open(ss_out_test_neg_nl_nostem.str());

      stringstream ss_out_test_neg_mrl;
      ss_out_test_neg_mrl << exp_dir << "/test_neg.mrl";
      out_test_neg_mrl.open(ss_out_test_neg_mrl.str());

      stringstream ss_out_test_neg_ori;
      ss_out_test_neg_ori << exp_dir << "/test_neg.ori";
      out_test_neg_ori.open(ss_out_test_neg_ori.str());

      stringstream ss_out_test_neg_gold;
      ss_out_test_neg_gold << exp_dir << "/test_neg.gold";
      out_test_neg_gold.open(ss_out_test_neg_gold.str());
    }

    string nl;
    string mrl;
    string gold;
    string data_dir = config.detailed_at("data_dir");
    string train = config.detailed_at("train");
    string tune = config.detailed_at("tune");
    string test = config.detailed_at("test");
    string test_neg = config.detailed_at("neg");
    string lang = config.detailed_at("lang");
    vector<string> both_nl;
    vector<string> both_mrl;

    // extract train
    stringstream ss_in_train_nl;
    ss_in_train_nl << data_dir << "/" << train << "." << lang;
    cout << "ss_in_train_nl: " << ss_in_train_nl.str() << endl;
    ifstream in_train_nl(ss_in_train_nl.str());
    while(getline(in_train_nl, nl)){
      preprocessed_sentence ps = preprocess_nl(nl, config);
      nl = ps.sentence;
      out_train_nl << nl << endl;
      both_nl.push_back(nl);
    }
    in_train_nl.close();

    stringstream ss_in_train_mrl;
    ss_in_train_mrl << data_dir << "/" << train << ".mrl";
    ifstream in_train_mrl(ss_in_train_mrl.str());
    while(getline(in_train_mrl, mrl)){
      out_train_ori << mrl << endl;
      string lin = preprocess_mrl(mrl, false);
      out_train_mrl << lin << endl;
      out_train_mrl_lm << "<s> " << lin << " </s>" << endl;
      both_mrl.push_back(lin);
    }
    in_train_mrl.close();

    assert(both_nl.size()==both_mrl.size());

    for(int i=0; i < both_nl.size(); i++){
      out_train_both << both_nl[i] << " ||| " << both_mrl[i] << endl;
    }

    // extract tune
    stringstream ss_in_tune_nl;
    ss_in_tune_nl << data_dir << "/" << tune << "." << lang;
    ifstream in_tune_nl(ss_in_tune_nl.str());
    while(getline(in_tune_nl, nl)){
      preprocessed_sentence ps = preprocess_nl(nl, config);
      nl = ps.sentence;
      out_tune_nl << nl << endl;
    }
    in_tune_nl.close();

    stringstream ss_in_tune_mrl;
    ss_in_tune_mrl << data_dir << "/" << tune << ".mrl";
    ifstream in_tune_mrl(ss_in_tune_mrl.str());
    while(getline(in_tune_mrl, mrl)){
      string lin = preprocess_mrl(mrl, false);
      out_tune_mrl << lin << endl;
    }
    in_tune_mrl.close();

    stringstream ss_in_tune_gold;
    ss_in_tune_gold << data_dir << "/" << tune << ".gold";
    ifstream in_tune_gold(ss_in_tune_gold.str());
    while(getline(in_tune_gold, gold)){
      out_tune_gold << gold << endl;
    }
    in_tune_gold.close();

    // extract test
    stringstream ss_in_test_nl;
    ss_in_test_nl << data_dir << "/" << test << "." << lang;
    ifstream in_test_nl(ss_in_test_nl.str());
    int count=1;
    while(getline(in_test_nl, nl)){
      string nl_nostem = nl;
      preprocessed_sentence ps = preprocess_nl(nl, config);
      nl = ps.sentence;
      nl_nostem = ps.non_stemmed;
      out_test_nl_nostem << nl_nostem << endl;
      out_test_nl << nl << endl;
      count++;
    }
    in_test_nl.close();

    stringstream ss_in_test_mrl;
    ss_in_test_mrl << data_dir << "/" << test << ".mrl";
    ifstream in_test_mrl(ss_in_test_mrl.str());
    while(getline(in_test_mrl, mrl)){
      out_test_ori << mrl << endl;
      string lin = preprocess_mrl(mrl, false);
      out_test_mrl << lin << endl;
    }
    in_test_mrl.close();

    stringstream ss_in_test_gold;
    ss_in_test_gold << data_dir << "/" << test << ".gold";
    ifstream in_test_gold(ss_in_test_gold.str());
    while(getline(in_test_gold, gold)){
      out_test_gold << gold << endl;
    }
    in_test_gold.close();

    // extract neg test
    if(config.detailed_at("neg")!=""){
      stringstream ss_in_test_neg_nl;
      ss_in_test_neg_nl << data_dir << "/" << test_neg << "." << lang;
      ifstream in_test_neg_nl(ss_in_test_neg_nl.str());
      while(getline(in_test_neg_nl, nl)){
        string nl_nostem = nl;
        preprocessed_sentence ps = preprocess_nl(nl, config);
        nl = ps.sentence;
        nl_nostem = ps.non_stemmed;
        out_test_neg_nl_nostem << nl_nostem << endl;
        out_test_neg_nl << nl << endl;
      }
      in_test_neg_nl.close();

      stringstream ss_in_test_neg_mrl;
      ss_in_test_neg_mrl << data_dir << "/" << test_neg << ".mrl";
      ifstream in_test_neg_mrl(ss_in_test_neg_mrl.str());
      while(getline(in_test_neg_mrl, mrl)){
        out_test_neg_ori << mrl << endl;
        string lin = preprocess_mrl(mrl, false);
        out_test_neg_mrl << lin << endl;
      }
      in_test_neg_mrl.close();

      stringstream ss_in_test_neg_gold;
      ss_in_test_neg_gold << data_dir << "/" << test_neg << ".gold";
      ifstream in_test_neg_gold(ss_in_test_neg_gold.str());
      while(getline(in_test_neg_gold, gold)){
        out_test_neg_gold << gold << endl;
      }
      in_test_neg_gold.close();
    }

    // close all opened files
    out_train_nl.close();
    out_train_mrl.close();
    out_train_mrl_lm.close();
    out_train_ori.close();
    out_train_both.close();
    out_tune_nl.close();
    out_tune_mrl.close();
    out_tune_gold.close();
    out_test_nl.close();
    out_test_nl_nostem.close();
    out_test_mrl.close();
    out_test_ori.close();
    out_test_gold.close();
    out_test_neg_nl.close();
    out_test_neg_nl_nostem.close();
    out_test_neg_mrl.close();
    out_test_neg_ori.close();
    out_test_neg_gold.close();
  }

} // namespace smt_semparse

#endif
