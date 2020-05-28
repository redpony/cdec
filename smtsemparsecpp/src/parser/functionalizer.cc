#ifndef FUNCTIONALIZER_CC
#define	FUNCTIONALIZER_CC

#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>
#include <boost/lexical_cast.hpp>
#include <stack>

#include "smt_semparse_config.h"
#include "parse_nl.h"
#include "ff_register.h"
#include "decoder.h"

using namespace std;

namespace smt_semparse {

static string functionalize(string& mrl, SMTSemparseConfig& config, preprocessed_sentence& ps, parseResult& parse_result){
  stack<int> stack_arity;
  stringstream result;
  vector<string> words_raw;
  vector<string> words;
  boost::split(words_raw, mrl, boost::is_any_of(" "));

  //insert @
  if(config.detailed_at("insertat")=="true"){
    vector<string> stemmed_words;
    vector<string> non_stemmed_words;
    boost::split(stemmed_words, ps.sentence, boost::is_any_of(" "));
    boost::split(non_stemmed_words, ps.non_stemmed, boost::is_any_of(" "));
    int word_pos = -1;
    int word_pos_counter = 0;
    bool found_word_pos = false;
    for(vector<string>::iterator it = words_raw.begin(); it != words_raw.end(); ++it){
      word_pos = -1;
      found_word_pos = false;
      word_pos_counter = 0;
      if(!boost::contains(*it, ARITY_SEP)){
        for(vector<string>::iterator stemmed_word = stemmed_words.begin(); stemmed_word != stemmed_words.end(); ++stemmed_word, ++word_pos_counter){
          if(*it == *stemmed_word){
            word_pos = word_pos_counter;
          }
          if(word_pos != -1){
            found_word_pos = true;
            stringstream new_word;
            string& non_stemmed_word = non_stemmed_words[word_pos];
            new_word << non_stemmed_word << "@s";
            words.push_back(new_word.str());
            word_pos = -1;
          }
        }
      }
      if(!found_word_pos){
        words.push_back(*it);
      }
    }
  } else {
    words = words_raw;
  }

  stringstream ss_recover_query;
  stringstream ss_recover_fun;
  string prev = "";
  for(vector<string>::iterator it = words.begin(); it != words.end(); ++it){
    ss_recover_fun << *it << " ";
    if(boost::contains(*it, ARITY_SEP)){
      vector<string> text_and_arity;
      boost::split(text_and_arity, *it, boost::is_any_of("@"));
      if(text_and_arity.size()!=2){
        cerr << "Warning: more than 1 ARITY_SEP in following mrl: " << mrl << endl;
        return "";
      }
      string& text = text_and_arity[0];
      string& arity_text = text_and_arity[1];
      ss_recover_query << text << " ";
      int arity;
      bool arity_str = false;
      if(arity_text==ARITY_STR){
        arity = -1;
        arity_str = true;
      } else {
        try{
          arity = boost::lexical_cast<int>(arity_text);
        } catch (boost::bad_lexical_cast) {
          cerr << "Arity is not a integer in following mrl: " << mrl << endl;
          return "";
        }
      }
      if(arity > 0){
        result << text << "(";
        stack_arity.push(arity);
      } else {
        if(arity == -1 && stack_arity.size() == 0){
          cerr << "Warning: stack size is 0 when it shouldn't be" << endl;
          return "";
        }
        //if the prev token is keyval, then we have a key here which also needs to be wrapped in '
        if(arity_str || prev == "keyval" || prev == "findkey"){ //findkey is unnecessary if it only holds a key but if there is additionally a topx then we need that ehre
          boost::replace_all(text, "€", " ");
          stringstream add_quotes;
          add_quotes << "'" << text << "'";
          text = add_quotes.str();
        }
        result << text;
        while(stack_arity.size() > 0){
          int top = stack_arity.top();
          stack_arity.pop();
          if(top > 1){
            result << ",";
            stack_arity.push(top - 1);
            break;
          } else {
            result << ")";
          }
        }
      }
      /*if(stack_arity.size() == 0){
        cerr << "Warning: stack size is 0 when it shouldn't be" << endl;
        return "";
      }*/
      prev = text; //need this if the prev token is keyval, then we have a key here which also needs to be wrapped in '
    } else {
      cerr << "Warning: no ARITY_SEP in following mrl: " << mrl << endl;
      return "";
    }

  }
  if(stack_arity.size() != 0){
    cerr << "Warning: stack size is not 0 when it should be: " << mrl << endl;
    return "";
  }

  if(config.detailed_at("cfg")=="true"){
    //valid mrl / parse via cdec
    string result_string = result.str();
    //hack
    //sequence of characters that does not contain ( or ) : [^\\(\\)]
    //result_string = boost::regex_replace(result_string, boost::regex("type\\("), ",type(");
    result_string = boost::regex_replace(result_string, boost::regex("\\("), "( ");
    result_string = boost::regex_replace(result_string, boost::regex(","), " , ");
    result_string = boost::regex_replace(result_string, boost::regex("\\)"), " )");
    result_string = boost::regex_replace(result_string, boost::regex("name:.*? \\)"), "name:lg )");
    result_string = boost::regex_replace(result_string, boost::regex("keyval\\( '([^\\(\\)]+?)' , '[^\\(\\)]+?' "), "keyval( '$1' , 'variablehere' "); // the comma is there to ensure that only -value- positions are replaced with variablehere
    result_string = boost::regex_replace(result_string, boost::regex("keyval\\( '([^\\(\\)]+?)' , or\\( '[^\\(\\)]+?' , '[^\\(\\)]+?' "), "keyval( '$1' , or( 'variablehere' , 'variablehere' "); //nasty hack for when we have a or() around values:  or( ' greek ' , ' variablehere ' )
    result_string = boost::regex_replace(result_string, boost::regex("keyval\\( '([^\\(\\)]+?)' , and\\( '[^\\(\\)]+?' , '[^\\(\\)]+?' "), "keyval( '$1' , and( 'variablehere' , 'variablehere' "); //nasty hack for when we have a and() around values:  and( ' greek ' , ' variablehere ' )
    result_string = boost::regex_replace(result_string, boost::regex(" '(.*?)' "), " ' $1 ' ");
    //split up numbers into individual digits
    boost::smatch match;
    if(boost::regex_search(result_string, match, boost::regex("topx\\( (.*?) \\)"))){
      string number = match[1];
      stringstream split_up_digits;
      for(string::iterator it = number.begin(); it != number.end(); ++it){
        split_up_digits << *it << " ";
      }
      result_string = boost::regex_replace(result_string, boost::regex("topx\\( .*?\\)"), "topx( "+split_up_digits.str()+")");
    }
    if(boost::regex_search(result_string, match, boost::regex("maxdist\\( ([0-9]+?) \\)"))){
      string number = match[1];
      stringstream split_up_digits;
      for(string::iterator it = number.begin(); it != number.end(); ++it){
        split_up_digits << *it << " ";
      }
      result_string = boost::regex_replace(result_string, boost::regex("maxdist\\( .*?\\)"), "maxdist( "+split_up_digits.str()+")");
    }

    stringstream ss_config;
    ss_config << "formalism=scfg" << endl;
    ss_config << "intersection_strategy=cube_pruning" << endl;
    ss_config << "cubepruning_pop_limit=1000" << endl;
    ss_config << "grammar=" << config.detailed_at("data_dir") << "/cfg/cfg_grammar_open_new_mrl" << endl;
    ss_config << "scfg_max_span_limit=1000" << endl;
    istringstream  config_file(ss_config.str());
    bool parse = true;
    Decoder decoder_validate(&config_file);
    cerr << "result_string: " << result_string << endl;
    decoder_validate.Decode(result_string, NULL, NULL, &parse);
    if(!parse){
      return "";
    }
  }

  string recover_query = ss_recover_query.str();
  boost::replace_all(recover_query, "SAVEAPO", "'");
  boost::replace_all(recover_query, "BRACKETOPEN", "(");
  boost::replace_all(recover_query, "BRACKETCLOSE", ")");

  string return_result = result.str();
  boost::replace_all(return_result, "SAVEAPO", "'");
  boost::replace_all(return_result, "BRACKETOPEN", "(");
  boost::replace_all(return_result, "BRACKETCLOSE", ")");

  parse_result.recover_query = recover_query;
  boost::trim(parse_result.recover_query);
  parse_result.recover_fun = ss_recover_fun.str();
  boost::trim(parse_result.recover_fun);
  return return_result;
}

static void functionalize_kbest(vector<string>& kbest_list, SMTSemparseConfig& config, preprocessed_sentence& ps, parseResult& parse_result){
  parse_result.mrl = "";
  parse_result.recover_query = "";
  for(vector<string>::iterator it = kbest_list.begin(); it != kbest_list.end(); ++it) {
    string fun = functionalize(*it, config, ps, parse_result);
    if(fun!=""){
      parse_result.mrl = fun;
      break;
    }
  }
}


} // namespace smt_semparse

#endif
