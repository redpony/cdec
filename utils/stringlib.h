#ifndef CDEC_STRINGLIB_H_
#define CDEC_STRINGLIB_H_

#ifdef STRINGLIB_DEBUG
#include <iostream>
#define SLIBDBG(x) do { std::cerr<<"DBG(stringlib): "<<x<<std::endl; } while(0)
#else
#define SLIBDBG(x)
#endif

#include <map>
#include <vector>
#include <cctype>
#include <cstring>
#include <string>
#include <sstream>
#include <algorithm>

namespace {
const char c_isspace[]=" \t\n\r\f\v"; // somewhat ridiculous, including characters nobody uses.
const char common_isspace[]=" \t\n\r"; // even \n\r is borderline, but maybe you pass multiline DOS format text.
}

inline std::size_t skip_ws(std::string const& s,std::size_t starting=0,char const* ws=common_isspace) {
  return s.find_first_not_of(ws,starting);
}

// returns position of end of all non-ws chars before ending, i.e. string(s.begin()+skip_ws(s),s.begin()+trailing_ws(s)) strips both ends
inline std::size_t trailing_ws(std::string const& s,std::size_t ending=std::string::npos,char const* ws=common_isspace) {
  std::size_t n=s.find_last_not_of(ws,ending);
  if (n==std::string::npos) return n;
  else return n+1;
}


inline bool is_single_line(std::string const& line) {
  return std::count(line.begin(),line.end(),'\n')==0; // but we want to allow terminal newlines/blanks
}

// is_single_line(strip_ws(line))
inline bool is_single_line_stripped(std::string const& line) {
  std::size_t b=skip_ws(line),e=trailing_ws(line);
  std::size_t n=line.find('\n',b);
  return n==std::string::npos || n>=e;
}

struct toupperc {
  inline char operator()(char c) const {
    return std::toupper(c);
  }
};

inline std::string toupper(std::string s) {
  std::transform(s.begin(),s.end(),s.begin(),toupperc());
  return s;
}

template <class Istr, class Isubstr> inline
bool match_begin(Istr bstr,Istr estr,Isubstr bsub,Isubstr esub)
{
  while (bsub != esub) {
    if (bstr == estr)
      return false;
    if (*bsub++ != *bstr++)
      return false;
  }
  return true;
}

template <class Istr, class Prefix> inline
bool match_begin(Istr bstr,Istr estr,Prefix prefix)
{
  return match_begin(bstr,estr,prefix.begin(),prefix.end());
}

template <class Str, class Prefix> inline
bool match_begin(Str const& str,Prefix const& prefix)
{
  return match_begin(str.begin(),str.end(),prefix.begin(),prefix.end());
}


// read line in the form of either:
//   source
//   source ||| target
// source will be returned as a string, target must be a sentence or
// a lattice (in PLF format) and will be returned as a Lattice object
void ParseTranslatorInput(const std::string& line, std::string* input, std::string* ref);
class Lattice;
void ParseTranslatorInputLattice(const std::string& line, std::string* input, Lattice* ref);

inline std::string Trim(const std::string& str, const std::string& dropChars = " \t") {
  std::string res = str;
  res.erase(str.find_last_not_of(dropChars)+1);
  return res.erase(0, res.find_first_not_of(dropChars));
}

inline void Tokenize(const std::string& str, char delimiter, std::vector<std::string>* res) {
  std::string s = str;
  unsigned last = 0;
  res->clear();
  for (unsigned i=0; i < s.size(); ++i)
    if (s[i] == delimiter) {
      s[i]=0;
      if (last != i) {
        res->push_back(&s[last]);
      }
      last = i + 1;
    }
  if (last != s.size())
    res->push_back(&s[last]);
}

inline unsigned NTokens(const std::string& str, char delimiter)
{
  std::vector<std::string> r;
  Tokenize(str,delimiter,&r);
  return r.size();
}

inline std::string LowercaseString(const std::string& in) {
  std::string res(in.size(),' ');
  for (unsigned i = 0; i < in.size(); ++i)
    res[i] = tolower(in[i]);
  return res;
}

inline std::string UppercaseString(const std::string& in) {
  std::string res(in.size(),' ');
  for (unsigned i = 0; i < in.size(); ++i)
    res[i] = toupper(in[i]);
  return res;
}

inline int CountSubstrings(const std::string& str, const std::string& sub) {
  size_t p = 0;
  int res = 0;
  while (p < str.size()) {
    p = str.find(sub, p);
    if (p == std::string::npos) break;
    ++res;
    p += sub.size();
  }
  return res;
}

inline int SplitOnWhitespace(const std::string& in, std::vector<std::string>* out) {
  out->clear();
  unsigned i = 0;
  unsigned start = 0;
  std::string cur;
  while(i < in.size()) {
    if (in[i] == ' ' || in[i] == '\t') {
      if (i - start > 0)
        out->push_back(in.substr(start, i - start));
      start = i + 1;
    }
    ++i;
  }
  if (i > start)
    out->push_back(in.substr(start, i - start));
  return out->size();
}

inline std::vector<std::string> SplitOnWhitespace(std::string const& in)
{
  std::vector<std::string> r;
  SplitOnWhitespace(in,&r);
  return r;
}


struct mutable_c_str {
  // because making a copy of a string might not copy its storage, so modifying a c_str() could screw up original (nobody uses cow nowadays because it needs locking under threading)
  char *p;
  mutable_c_str(std::string const& s) : p((char *)::operator new(s.size()+1)) {
    std::memcpy(p,s.data(),s.size());
    p[s.size()]=0;
  }
  ~mutable_c_str() { ::operator delete(p); }
private:
  mutable_c_str(mutable_c_str const&);
};

// ' ' '\t' tokens hardcoded
//NOTE: you should have stripped endline chars out first.
inline bool IsWordSep(char c) {
  return c==' '||c=='\t';
}


template <class F>
// *end must be 0 (i.e. [p,end] is valid storage, which will be written to with 0 to separate c string tokens
void VisitTokens(char *p,char *const end,F f) {
  SLIBDBG("VisitTokens. p="<<p<<" Nleft="<<end-p);
  if (p==end) return;
  char *last; // 0 terminated already.  this is ok to mutilate because s is a copy of the string passed in.  well, barring copy on write i guess.
  while(IsWordSep(*p)) { ++p;if (p==end) return; } // skip init whitespace
  last=p; // first non-ws char
  for(;;) {
    SLIBDBG("Start of word. last="<<last<<" *p="<<*p<<" Nleft="<<end-p);
    // last==p, pointing at first non-ws char not yet translated into f(word) call
    for(;;) {// p to end of word
      ++p;
      if (p==end) {
        f(last);
        SLIBDBG("Returning. word="<<last<<" *p="<<*p<<" Nleft="<<end-p);
        return;
      }
      if (IsWordSep(*p)) break;
    }
    *p=0;
    f(last);
    SLIBDBG("End of word. word="<<last<<" rest="<<p+1<<" Nleft="<<end-p);
    for(;;) { // again skip extra whitespace
      ++p;
      if (p==end) return;
      if (!IsWordSep(*p)) break;
    }
    last=p;
  }
}

template <class F>
void VisitTokens(char *p,F f) {
  VisitTokens(p,p+std::strlen(p),f);
}


template <class F>
void VisitTokens(std::string const& s,F f) {
  if (0) {
  std::vector<std::string> ss=SplitOnWhitespace(s);
  for (unsigned i=0;i<ss.size();++i)
    f(ss[i]);
  return;
  }
  //FIXME:
  if (s.empty()) return;
  mutable_c_str mp(s);
  SLIBDBG("mp="<<mp.p);
  VisitTokens(mp.p,mp.p+s.size(),f);
}

inline void SplitCommandAndParam(const std::string& in, std::string* cmd, std::string* param) {
  cmd->clear();
  param->clear();
  std::vector<std::string> x;
  SplitOnWhitespace(in, &x);
  if (x.size() == 0) return;
  *cmd = x[0];
  for (unsigned i = 1; i < x.size(); ++i) {
    if (i > 1) { *param += " "; }
    *param += x[i];
  }
}

void ProcessAndStripSGML(std::string* line, std::map<std::string, std::string>* out);
std::string SGMLOpenSegTag(const std::map<std::string, std::string>& attr);

// given the first character of a UTF8 block, find out how wide it is
// see http://en.wikipedia.org/wiki/UTF-8 for more info
inline unsigned int UTF8Len(unsigned char x) {
  if (x < 0x80) return 1;
  else if ((x >> 5) == 0x06) return 2;
  else if ((x >> 4) == 0x0e) return 3;
  else if ((x >> 3) == 0x1e) return 4;
  else return 0;
}

inline unsigned int UTF8StringLen(const std::string& x) {
  unsigned pos = 0;
  int len = 0;
  while(pos < x.size()) {
    ++len;
    pos += UTF8Len(x[pos]);
  }
  return len;
}

std::string md5(const std::string& in);

#endif
