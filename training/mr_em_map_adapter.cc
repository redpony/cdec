#include <iostream>
#include <fstream>
#include <cassert>
#include <cmath>

#include <boost/utility.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>
#include "boost/tuple/tuple.hpp"

#include "fdict.h"
#include "sparse_vector.h"

using namespace std;
namespace po = boost::program_options;

// useful for EM models parameterized by a bunch of multinomials
// this converts event counts (returned from cdec as feature expectations)
// into different keys and values (which are lists of all the events,
// conditioned on the key) for summing and normalization by a reducer

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("buffer_size,b", po::value<int>()->default_value(1), "Buffer size (in # of counts) before emitting counts")
        ("format,f",po::value<string>()->default_value("b64"), "Encoding of the input (b64 or text)");
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

struct EventMapper {
  int Map(int fid) {
    int& cv = map_[fid];
    if (!cv) {
      cv = GetConditioningVariable(fid);
    }
    return cv;
  }
  void Clear() { map_.clear(); }
 protected:
  virtual int GetConditioningVariable(int fid) const = 0;
 private:
  map<int, int> map_;
};

struct LexAlignEventMapper : public EventMapper {
 protected:
  virtual int GetConditioningVariable(int fid) const {
    const string& str = FD::Convert(fid);
    size_t pos = str.rfind("_");
    if (pos == string::npos || pos == 0 || pos >= str.size() - 1) {
      cerr << "Bad feature for EM adapter: " << str << endl;
      abort();
    }
    return FD::Convert(str.substr(0, pos));
  }
};

int main(int argc, char** argv) {
  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);

  const bool use_b64 = conf["format"].as<string>() == "b64";
  const int buffer_size = conf["buffer_size"].as<int>();

  const string s_obj = "**OBJ**";
  // 0<TAB>**OBJ**=12.2;Feat1=2.3;Feat2=-0.2;
  // 0<TAB>**OBJ**=1.1;Feat1=1.0;

  EventMapper* event_mapper = new LexAlignEventMapper;
  map<int, SparseVector<double> > counts;
  size_t total = 0;
  while(cin) {
    string line;
    getline(cin, line);
    if (line.empty()) continue;
    int feat;
    double val;
    size_t i = line.find("\t");
    assert(i != string::npos);
    ++i;
    SparseVector<double> g;
    double obj = 0;
    if (use_b64) {
      if (!B64::Decode(&obj, &g, &line[i], line.size() - i)) {
        cerr << "B64 decoder returned error, skipping!\n";
        continue;
      }
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
          obj = val;
        } else {
          g.set_value(feat, val);
        }
      }
    }
    //cerr << "OBJ: " << obj << endl;
    const SparseVector<double>& cg = g;
    for (SparseVector<double>::const_iterator it = cg.begin(); it != cg.end(); ++it) {
      const int cond_var = event_mapper->Map(it->first);
      SparseVector<double>& cond_counts = counts[cond_var];
      int delta = cond_counts.size();
      cond_counts.add_value(it->first, it->second);
      delta = cond_counts.size() - delta;
      total += delta;
    }
    if (total > buffer_size) {
      for (map<int, SparseVector<double> >::iterator it = counts.begin();
           it != counts.end(); ++it) {
        const SparseVector<double>& cc = it->second;
        cout << FD::Convert(it->first) << '\t';
        if (use_b64) {
          B64::Encode(0.0, cc, &cout);
        } else {
          abort();
        }
        cout << endl;
      }
      cout << flush;
      total = 0;
      counts.clear();
    }
  }

  return 0;
}

