#ifndef _DTRAIN_H_
#define _DTRAIN_H_

#undef DTRAIN_FASTER_PERCEPTRON // only look at misranked pairs
                                 // DO NOT USE WITH SVM!
#define DTRAIN_LOCAL
#define DTRAIN_DOTS 10 // after how many inputs to display a '.'
#define DTRAIN_GRAMMAR_DELIM "########EOS########"
#define DTRAIN_SCALE 100000


#include <iomanip>
#include <climits>
#include <string.h>

#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>

#include "ksampler.h"
#include "pairsampling.h"

#include "filelib.h"



using namespace std;
using namespace dtrain;
namespace po = boost::program_options;

inline void register_and_convert(const vector<string>& strs, vector<WordID>& ids)
{
  vector<string>::const_iterator it;
  for (it = strs.begin(); it < strs.end(); it++)
    ids.push_back(TD::Convert(*it));
}

inline string gettmpf(const string path, const string infix)
{
  char fn[path.size() + infix.size() + 8];
  strcpy(fn, path.c_str());
  strcat(fn, "/");
  strcat(fn, infix.c_str());
  strcat(fn, "-XXXXXX");
  if (!mkstemp(fn)) {
    cerr << "Cannot make temp file in" << path << " , exiting." << endl;
    exit(1);
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

struct HSReporter
{
  string task_id_;

  HSReporter(string task_id) : task_id_(task_id) {}

  inline void update_counter(string name, unsigned amount) {
    cerr << "reporter:counter:" << task_id_ << "," << name << "," << amount << endl;
  }
  inline void update_gcounter(string name, unsigned amount) {
    cerr << "reporter:counter:Global," << name << "," << amount << endl;
  }
};

inline ostream& _np(ostream& out) { return out << resetiosflags(ios::showpos); }
inline ostream& _p(ostream& out)  { return out << setiosflags(ios::showpos); }
inline ostream& _p2(ostream& out) { return out << setprecision(2); }
inline ostream& _p5(ostream& out) { return out << setprecision(5); }

inline void printWordIDVec(vector<WordID>& v)
{
  for (unsigned i = 0; i < v.size(); i++) {
    cerr << TD::Convert(v[i]);
    if (i < v.size()-1) cerr << " ";
  }
}

template<typename T>
inline T sign(T z)
{
  if (z == 0) return 0;
  return z < 0 ? -1 : +1;
}

#endif

