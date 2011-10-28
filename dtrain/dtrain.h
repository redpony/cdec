#ifndef _DTRAIN_COMMON_H_
#define _DTRAIN_COMMON_H_



#include <iomanip>
#include <climits>
#include <string.h>

#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>

#include "ksampler.h"
#include "pairsampling.h"

#include "filelib.h"

//#define DTRAIN_LOCAL

#define DTRAIN_DOTS 100 // when to display a '.'
#define DTRAIN_GRAMMAR_DELIM "########EOS########"

using namespace std;
using namespace dtrain;
namespace po = boost::program_options;

inline void register_and_convert(const vector<string>& strs, vector<WordID>& ids) {
  vector<string>::const_iterator it;
  for (it = strs.begin(); it < strs.end(); it++)
    ids.push_back(TD::Convert(*it));
}

inline string gettmpf(const string path, const string infix, const string suffix="") {
  char fn[1024];
  strcpy(fn, path.c_str());
  strcat(fn, "/");
  strcat(fn, infix.c_str());
  strcat(fn, "-XXXXXX");
  mkstemp(fn);
  if (suffix != "") { // we will get 2 files
    strcat(fn, ".");
    strcat(fn, suffix.c_str());
  }
  return string(fn);
}

inline void split_in(string& s, vector<string>& parts)
{
  unsigned f = 0;
  for(unsigned i = 0; i < 3; i++) {
    unsigned e = f;
    f = s.find("\t", f+1);
    if (e != 0) parts.push_back(s.substr(e+1, f-e-1));
    else parts.push_back(s.substr(0, f)); 
  }
  s.erase(0, f+1);
}

inline ostream& _np(ostream& out) { return out << resetiosflags(ios::showpos); }
inline ostream& _p(ostream& out)  { return out << setiosflags(ios::showpos); }
inline ostream& _p2(ostream& out) { return out << setprecision(2); }
inline ostream& _p5(ostream& out) { return out << setprecision(5); }
inline ostream& _p9(ostream& out) { return out << setprecision(9); }

inline void printWordIDVec(vector<WordID>& v)
{
  for (unsigned i = 0; i < v.size(); i++) {
    cerr << TD::Convert(v[i]);
    if (i < v.size()-1) cerr << " ";
  }
}

#endif

