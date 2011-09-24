#ifndef _DTRAIN_COMMON_H_
#define _DTRAIN_COMMON_H_


#include <iomanip>

#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>

#include "verbose.h"
#include "viterbi.h"
#include "ff_register.h"
#include "decoder.h"
#include "weights.h"

#include "score.h"
#include "kbestget.h"
#include "ksampler.h"
#include "pairsampling.h"

#define DTRAIN_DOTS 100                     // when to display a '.'
#define DTRAIN_TMP_DIR "/var/hadoop/mapred/local"               // put this on a SSD?
#define DTRAIN_GRAMMAR_DELIM "########EOS########"

using namespace std;
using namespace dtrain;
namespace po = boost::program_options;

inline void register_and_convert(const vector<string>& strs, vector<WordID>& ids) {
  vector<string>::const_iterator it;
  for (it = strs.begin(); it < strs.end(); it++)
    ids.push_back(TD::Convert(*it));
}
inline ostream& _np(ostream& out) { return out << resetiosflags(ios::showpos); }
inline ostream& _p(ostream& out)  { return out << setiosflags(ios::showpos); }
inline ostream& _p2(ostream& out) { return out << setprecision(2); }
inline ostream& _p5(ostream& out) { return out << setprecision(5); }
inline ostream& _p9(ostream& out) { return out << setprecision(9); }
inline void strsplit(string &s, vector<string>& v, char d = '\t', size_t parts = 0) { 
  stringstream ss(s);
  string t;
  size_t c = 0;
  while(true)
  {
    if (parts > 0 && c == parts-1) {
      getline(ss, t);
      v.push_back(t);
      break;
    }
    if (!getline(ss, t, d)) break;
    v.push_back(t);
    c++;
  }
}

#endif

