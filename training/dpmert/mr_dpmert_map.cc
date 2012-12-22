#include <sstream>
#include <iostream>
#include <fstream>
#include <vector>

#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "ns.h"
#include "ns_docscorer.h"
#include "ces.h"
#include "filelib.h"
#include "stringlib.h"
#include "sparse_vector.h"
#include "mert_geometry.h"
#include "inside_outside.h"
#include "error_surface.h"
#include "b64tools.h"
#include "hg_io.h"

using namespace std;
namespace po = boost::program_options;

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("reference,r",po::value<vector<string> >(), "[REQD] Reference translation (tokenized text)")
        ("source,s",po::value<string>(), "Source file (ignored, except for AER)")
        ("evaluation_metric,m",po::value<string>()->default_value("ibm_bleu"), "Evaluation metric being optimized")
        ("input,i",po::value<string>()->default_value("-"), "Input file to map (- is STDIN)")
        ("help,h", "Help");
  po::options_description dcmdline_options;
  dcmdline_options.add(opts);
  po::store(parse_command_line(argc, argv, dcmdline_options), *conf);
  bool flag = false;
  if (!conf->count("reference")) {
    cerr << "Please specify one or more references using -r <REF.TXT>\n";
    flag = true;
  }
  if (flag || conf->count("help")) {
    cerr << dcmdline_options << endl;
    exit(1);
  }
}

bool ReadSparseVectorString(const string& s, SparseVector<double>* v) {
#if 0
  // this should work, but untested.
  std::istringstream i(s);
  i>>*v;
#else
  vector<string> fields;
  Tokenize(s, ';', &fields);
  if (fields.empty()) return false;
  for (unsigned i = 0; i < fields.size(); ++i) {
    vector<string> pair(2);
    Tokenize(fields[i], '=', &pair);
    if (pair.size() != 2) {
      cerr << "Error parsing vector string: " << fields[i] << endl;
      return false;
    }
    v->set_value(FD::Convert(pair[0]), atof(pair[1].c_str()));
  }
  return true;
#endif
}

int main(int argc, char** argv) {
  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);
  const string evaluation_metric = conf["evaluation_metric"].as<string>();
  EvaluationMetric* metric = EvaluationMetric::Instance(evaluation_metric);
  DocumentScorer ds(metric, conf["reference"].as<vector<string> >());
  cerr << "Loaded " << ds.size() << " references for scoring with " << evaluation_metric << endl;
  Hypergraph hg;
  string last_file;
  ReadFile in_read(conf["input"].as<string>());
  istream &in=*in_read.stream();
  while(in) {
    string line;
    getline(in, line);
    if (line.empty()) continue;
    istringstream is(line);
    int sent_id;
    string file, s_origin, s_direction;
    // path-to-file (JSON) sent_ed starting-point search-direction
    is >> file >> sent_id >> s_origin >> s_direction;
    SparseVector<double> origin;
    ReadSparseVectorString(s_origin, &origin);
    SparseVector<double> direction;
    ReadSparseVectorString(s_direction, &direction);
    // cerr << "File: " << file << "\nDir: " << direction << "\n   X: " << origin << endl;
    if (last_file != file) {
      last_file = file;
      ReadFile rf(file);
      HypergraphIO::ReadFromJSON(rf.stream(), &hg);
    }
    const ConvexHullWeightFunction wf(origin, direction);
    const ConvexHull hull = Inside<ConvexHull, ConvexHullWeightFunction>(hg, NULL, wf);

    ErrorSurface es;
    ComputeErrorSurface(*ds[sent_id], hull, &es, metric, hg);
    //cerr << "Viterbi envelope has " << ve.size() << " segments\n";
    // cerr << "Error surface has " << es.size() << " segments\n";
    string val;
    es.Serialize(&val);
    cout << 'M' << ' ' << s_origin << ' ' << s_direction << '\t';
    B64::b64encode(val.c_str(), val.size(), &cout);
    cout << endl << flush;
  }
  return 0;
}
