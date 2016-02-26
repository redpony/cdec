#include <iostream>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include <boost/algorithm/string.hpp>

#include "nominatim_check.h"

using namespace std;

NominatimCheck::NominatimCheck(const char* url, int to) : requester(url, to) { }

int NominatimCheck::protect_sentence_for_nominatim(string* ptr_sentence, string* ptr_parallel_sentence) {
  string& sentence = (*ptr_sentence);
  vector<string> words;
  boost::split(words, sentence, boost::is_any_of(" "));
  vector<string> parallel_words;
  if(ptr_parallel_sentence!=NULL){
    string& parallel_sentence = (*ptr_parallel_sentence);
    boost::split(parallel_words, parallel_sentence, boost::is_any_of(" "));
    if(words.size() != parallel_words.size()){ cerr << "Parallel sentences need to have same number of words" << endl; exit(1);}
  }
  stringstream ss_assemble;
  stringstream ss_assemble_parallel;
  bool first = true;
  int count = 0;
  for(auto it = words.begin(); it != words.end(); ++it,++count){
    stringstream input;
    input << "<s>" << (*it) << "</s>";
    const char* result = requester.request_for_sentence(input.str().c_str());
    //cerr << "words: " << input.str() << " : " << result << endl;
    if(first){
      first = false;
    } else {
      ss_assemble << " ";
      ss_assemble_parallel << " ";
    }
    if((*result)=='1'){
      ss_assemble << "{nominatim:" << (*it) << "}";
      if(ptr_parallel_sentence!=NULL){
        ss_assemble_parallel << "{nominatim:" << parallel_words[count] << "}";
      }
    } else {
      ss_assemble << (*it);
      if(ptr_parallel_sentence!=NULL){
        ss_assemble_parallel << parallel_words[count];
      }
    }
  }
  sentence = ss_assemble.str();
  if(ptr_parallel_sentence!=NULL){
    (*ptr_parallel_sentence) = ss_assemble_parallel.str();
  }
  return 0;
}
