#ifndef CDEC_SENTENCES_H
#define CDEC_SENTENCES_H

#include <algorithm>
#include <vector>
#include <iostream>
#include "filelib.h"
#include "tdict.h"
#include "stringlib.h"
#include <cstring>
typedef std::vector<WordID> Sentence;

// these "iterators" are invalidated if s is modified.  note: this is allowed by std.
inline WordID const* begin(Sentence const& s) {
  return &*s.begin();
}
inline WordID const* end(Sentence const& s) {
  return &*s.end();
}
inline WordID * begin(Sentence & s) {
  return &*s.begin();
}
inline WordID * end(Sentence & s) {
  return &*s.end();
}
inline void wordcpy(WordID *dest,WordID const* src,int n) {
  std::memcpy(dest,src,n*sizeof(*dest));
}
inline void wordcpy(WordID *dest,WordID const* src,WordID const* src_end) {
  wordcpy(dest,src,src_end-src);
}
inline WordID *wordcpy_reverse(WordID *dest,WordID const* src,WordID const* src_end) {
  for(WordID const* i=src_end;i>src;)
    *dest++=*--i;
  return dest;
}
inline Sentence singleton_sentence(WordID t) {
  return Sentence(1,t);
}

inline Sentence singleton_sentence(std::string const& s) {
  return singleton_sentence(TD::Convert(s));
}


inline std::ostream & operator<<(std::ostream &out,Sentence const& s) {
  return out<<TD::GetString(s);
}

inline void StringToSentence(std::string const& str,Sentence &s) {
  using namespace std;
  s.clear();
  TD::ConvertSentence(str,&s);
/*  vector<string> ss=SplitOnWhitespace(str);
  transform(ss.begin(),ss.end(),back_inserter(s),ToTD());
*/

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
