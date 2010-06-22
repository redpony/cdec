#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>

#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "config.h"
#ifdef HAVE_BOOST_DIGAMMA
#include <boost/math/special_functions/digamma.hpp>
using boost::math::digamma;
#endif

#include "filelib.h"
#include "fdict.h"
#include "weights.h"
#include "sparse_vector.h"

using namespace std;
namespace po = boost::program_options;

#ifndef HAVE_BOOST_DIGAMMA
#warning Using Mark Johnsons digamma()
double digamma(double x) {
  double result = 0, xx, xx2, xx4;
  assert(x > 0);
  for ( ; x < 7; ++x)
    result -= 1/x;
  x -= 1.0/2.0;
  xx = 1.0/x;
  xx2 = xx*xx;
  xx4 = xx2*xx2;
  result += log(x)+(1./24.)*xx2-(7.0/960.0)*xx4+(31.0/8064.0)*xx4*xx2-(127.0/30720.0)*xx4*xx4;
  return result;
}
#endif

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("optimization_method,m", po::value<string>()->default_value("em"), "Optimization method (em, vb)")
        ("input_format,f",po::value<string>()->default_value("b64"),"Encoding of the input (b64 or text)");
  po::options_description clo("Command line options");
  clo.add_options()
        ("config", po::value<string>(), "Configuration file")
        ("help,h", "Print this help message and exit");
  po::options_description dconfig_options, dcmdline_options;
  dconfig_options.add(opts);
  dcmdline_options.add(opts).add(clo);
  
  po::store(parse_command_line(argc, argv, dcmdline_options), *conf);
  if (conf->count("config")) {
    ifstream config((*conf)["config"].as<string>().c_str());
    po::store(po::parse_config_file(config, dconfig_options), *conf);
  }
  po::notify(*conf);

  if (conf->count("help")) {
    cerr << dcmdline_options << endl;
    exit(1);
  }
}

double NoZero(const double& x) {
  if (x) return x;
  return 1e-35;
}

void Maximize(const bool use_vb,
              const double& alpha,
              const int total_event_types,
              SparseVector<double>* pc) {
  const SparseVector<double>& counts = *pc;

  if (use_vb)
    assert(total_event_types >= counts.num_active());

  double tot = 0;
  for (SparseVector<double>::const_iterator it = counts.begin();
       it != counts.end(); ++it)
    tot += it->second;
//  cerr << " = " << tot << endl;
  assert(tot > 0.0);
  double ltot = log(tot);
  if (use_vb)
    ltot = digamma(tot + total_event_types * alpha);
  for (SparseVector<double>::const_iterator it = counts.begin();
       it != counts.end(); ++it) {
    if (use_vb) {
      pc->set_value(it->first, NoZero(digamma(it->second + alpha) - ltot));
    } else {
      pc->set_value(it->first, NoZero(log(it->second) - ltot));
    }
  }
#if 0
  if (counts.num_active() < 50) {
    for (SparseVector<double>::const_iterator it = counts.begin();
         it != counts.end(); ++it) {
      cerr << " p(" << FD::Convert(it->first) << ")=" << exp(it->second);
    }
    cerr << endl;
  }
#endif
}

int main(int argc, char** argv) {
  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);

  const bool use_b64 = conf["input_format"].as<string>() == "b64";
  const bool use_vb = conf["optimization_method"].as<string>() == "vb";
  const double alpha = 1e-09;
  if (use_vb)
    cerr << "Using variational Bayes, make sure alphas are set\n";

  const string s_obj = "**OBJ**";
  // E-step
  string cur_key = "";
  SparseVector<double> acc;
  double logprob = 0;
  while(cin) {
    string line;
    getline(cin, line);
    if (line.empty()) continue;
    int feat;
    double val;
    size_t i = line.find("\t");
    const string key = line.substr(0, i);
    assert(i != string::npos);
    ++i;
    if (key != cur_key) {
      if  (cur_key.size() > 0) {
        // TODO shouldn't be num_active, should be total number
        // of events
        Maximize(use_vb, alpha, acc.num_active(), &acc);
        cout << cur_key << '\t';
        if (use_b64)
          B64::Encode(0.0, acc, &cout);
        else
          cout << acc;
        cout << endl;
        acc.clear();
      }
      cur_key = key;
    }
    if (use_b64) {
      SparseVector<double> g;
      double obj;
      if (!B64::Decode(&obj, &g, &line[i], line.size() - i)) {
        cerr << "B64 decoder returned error, skipping!\n";
        continue;
      }
      logprob += obj;
      acc += g;
    } else {       // text encoding - your counts will not be accurate!
      while (i < line.size()) {
        size_t start = i;
        while (line[i] != '=' && i < line.size()) ++i;
        if (i == line.size()) { cerr << "FORMAT ERROR\n"; break; }
        string fname = line.substr(start, i - start);
        if (fname == s_obj) {
          feat = -1;
        } else {
          feat = FD::Convert(line.substr(start, i - start));
        }
        ++i;
        start = i;
        while (line[i] != ';' && i < line.size()) ++i;
        if (i - start == 0) continue;
        val = atof(line.substr(start, i - start).c_str());
        ++i;
        if (feat == -1) {
          logprob += val;
        } else {
          acc.add_value(feat, val);
        }
      }
    }
  }
  // TODO shouldn't be num_active, should be total number
  // of events
  Maximize(use_vb, alpha, acc.num_active(), &acc);
  cout << cur_key << '\t';
  if (use_b64)
    B64::Encode(0.0, acc, &cout);
  else
    cout << acc;
  cout << endl << flush;

  cerr << "LOGPROB: " << logprob << endl;

  return 0;
}
