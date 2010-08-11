#include <sstream>
#include <iostream>
#include <fstream>
#include <vector>

#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "ces.h"
#include "filelib.h"
#include "stringlib.h"
#include "sparse_vector.h"
#include "scorer.h"
#include "viterbi_envelope.h"
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
        ("loss_function,l",po::value<string>()->default_value("ibm_bleu"), "Loss function being optimized")
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
  vector<string> fields;
  Tokenize(s, ';', &fields);
  if (fields.empty()) return false;
  for (int i = 0; i < fields.size(); ++i) {
    vector<string> pair(2);
    Tokenize(fields[i], '=', &pair);
    if (pair.size() != 2) {
      cerr << "Error parsing vector string: " << fields[i] << endl;
      return false;
    }
    v->set_value(FD::Convert(pair[0]), atof(pair[1].c_str()));
  }
  return true;
}

int main(int argc, char** argv) {
  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);
  const string loss_function = conf["loss_function"].as<string>();
  ScoreType type = ScoreTypeFromString(loss_function);
  DocScorer ds(type, conf["reference"].as<vector<string> >(), conf["source"].as<string>());
  cerr << "Loaded " << ds.size() << " references for scoring with " << loss_function << endl;
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
    string file, s_origin, s_axis;
    // path-to-file (JSON) sent_ed starting-point search-direction
    is >> file >> sent_id >> s_origin >> s_axis;
    SparseVector<double> origin;
    assert(ReadSparseVectorString(s_origin, &origin));
    SparseVector<double> axis;
    assert(ReadSparseVectorString(s_axis, &axis));
    // cerr << "File: " << file << "\nAxis: " << axis << "\n   X: " << origin << endl;
    if (last_file != file) {
      last_file = file;
      ReadFile rf(file);
      HypergraphIO::ReadFromJSON(rf.stream(), &hg);
    }
    ViterbiEnvelopeWeightFunction wf(origin, axis);
    ViterbiEnvelope ve = Inside<ViterbiEnvelope, ViterbiEnvelopeWeightFunction>(hg, NULL, wf);
    ErrorSurface es;
    ComputeErrorSurface(*ds[sent_id], ve, &es, type, hg);
    //cerr << "Viterbi envelope has " << ve.size() << " segments\n";
    // cerr << "Error surface has " << es.size() << " segments\n";
    string val;
    es.Serialize(&val);
    cout << 'M' << ' ' << s_origin << ' ' << s_axis << '\t';
    B64::b64encode(val.c_str(), val.size(), &cout);
    cout << endl << flush;
  }
  return 0;
}
