#ifndef SPELL_CHECKER_CC
#define	SPELL_CHECKER_CC

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstring>
#include <sstream>

#include "hunspell/hunspell.hxx"

using namespace std;

static string correct_file(vector<string>& words){
  int dp;
  char* buf;
  stringstream new_sentence;
  bool first = true;

  Hunspell* pMS = new Hunspell("/usr/share/hunspell/en_US.aff",
                               "/usr/share/hunspell/en_US.dic");

  for(auto it = words.begin(); it != words.end(); ++it){
    buf = strdup((*it).c_str());
    if(!first){ new_sentence << " "; }
    if((*it) == "?" || (*it) == "." || (*it) == ";" || (*it) == "," || (*it) == "!" || (*it) == "'" || (*it) == "-"){ //don't spell check special characters
      new_sentence << (*it);
      continue;
    }
    dp = pMS->spell(buf);
    if (dp) {
      new_sentence << string(buf);
    } else {
      char** wlst;
      int ns = pMS->suggest(&wlst, buf);
      for (int i = 0; i < ns; i++) {
        new_sentence << string(wlst[i]);
        break;
      }
      pMS->free_list(&wlst, ns);
    }
    first = false;
  }

  delete pMS;
  return new_sentence.str();
}

#endif
