#ifndef CDEC_SENTENCES_H
#define CDEC_SENTENCES_H

#include <algorithm>
#include <vector>
#include <iostream>
#include "filelib.h"
#include "tdict.h"
#include "stringlib.h"
typedef std::vector<WordID> Sentence;

inline void StringToSentence(std::string const& str,Sentence &s) {
  using namespace std;
  vector<string> ss=SplitOnWhitespace(str);
  s.clear();
  transform(ss.begin(),ss.end(),back_inserter(s),ToTD());
}

inline Sentence StringToSentence(std::string const& str) {
  Sentence s;
  StringToSentence(str,s);
  return s;
}

inline std::istream& operator >> (std::istream &in,Sentence &s) {
  using namespace std;
  string str;
  if (getline(in,str)) {
    StringToSentence(str,s);
  }
  return in;
}


class Sentences : public std::vector<Sentence> {
  typedef std::vector<Sentence> VS;
public:
  Sentences() {  }
  Sentences(unsigned n,Sentence const& sentence) : VS(n,sentence) {  }
  Sentences(unsigned n,std::string const& sentence) : VS(n,StringToSentence(sentence)) {  }
  void Load(std::string file) {
    ReadFile r(file);
    Load(*r.stream());
  }
  void Load(std::istream &in) {
    this->push_back(Sentence());
    while(in>>this->back()) ;
    this->pop_back();
  }
};


#endif
