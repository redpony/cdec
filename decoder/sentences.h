#ifndef CDEC_SENTENCES_H
#define CDEC_SENTENCES_H

#include <algorithm>
#include <vector>
#include <iostream>
#include "filelib.h"
#include "tdict.h"
#include "stringlib.h"
typedef std::vector<WordID> Sentence;

inline std::ostream & operator<<(std::ostream &out,Sentence const& s) {
  return out<<TD::GetString(s);
}

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
  std::string filename;
  void Load(std::string file) {
    ReadFile r(file);
    Load(r.get(),file);
  }
  void Load(std::istream &in,std::string filen="-") {
    filename=filen;
    do {
      this->push_back(Sentence());
    } while(in>>this->back());
    this->pop_back();
  }
  void Print(std::ostream &out,int headn=0) const {
    out << "[" << size()<< " sentences from "<<filename<<"]";
    if (headn!=0) {
      int i=0,e=this->size();
      if (headn>0&&headn<e) {
        e=headn;
        out << " (first "<<headn<<")";
      }
      out << " :\n";
      for (;i<e;++i)
        out<<(*this)[i] << "\n";
    }
  }
  friend inline std::ostream& operator<<(std::ostream &out,Sentences const& s) {
    s.Print(out);
    return out;
  }
};


#endif
