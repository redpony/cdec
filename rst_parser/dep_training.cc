#include "dep_training.h"

#include <vector>
#include <iostream>

#include "stringlib.h"
#include "filelib.h"
#include "tdict.h"
#include "picojson.h"

using namespace std;

static void ParseInstance(const string& line, int start, TrainingInstance* out, int lc = 0) {
  picojson::value obj;
  string err;
  picojson::parse(obj, line.begin() + start, line.end(), &err);
  if (err.size() > 0) { cerr << "JSON parse error in " << lc << ": " << err << endl; abort(); }
  TrainingInstance& cur = *out;
  TaggedSentence& ts = cur.ts;
  EdgeSubset& tree = cur.tree;
  ts.pos.clear();
  ts.words.clear();
  tree.roots.clear();
  tree.h_m_pairs.clear();
  assert(obj.is<picojson::object>());
  const picojson::object& d = obj.get<picojson::object>();
  const picojson::array& ta = d.find("tokens")->second.get<picojson::array>();
  for (unsigned i = 0; i < ta.size(); ++i) {
    ts.words.push_back(TD::Convert(ta[i].get<picojson::array>()[0].get<string>()));
    ts.pos.push_back(TD::Convert(ta[i].get<picojson::array>()[1].get<string>()));
  }
  if (d.find("deps") != d.end()) {
    const picojson::array& da = d.find("deps")->second.get<picojson::array>();
    for (unsigned i = 0; i < da.size(); ++i) {
      const picojson::array& thm = da[i].get<picojson::array>();
      // get dep type here
      short h = thm[2].get<double>();
      short m = thm[1].get<double>();
      if (h < 0)
        tree.roots.push_back(m);
      else
        tree.h_m_pairs.push_back(make_pair(h,m));
    }
  }
  //cerr << TD::GetString(ts.words) << endl << TD::GetString(ts.pos) << endl << tree << endl;
}

bool TrainingInstance::ReadInstance(std::istream* in, TrainingInstance* instance) {
  string line;
  if (!getline(*in, line)) return false;
  size_t pos = line.rfind('\t');
  assert(pos != string::npos);
  static int lc = 0; ++lc;
  ParseInstance(line, pos + 1, instance, lc);
  return true;
}

void TrainingInstance::ReadTrainingCorpus(const string& fname, vector<TrainingInstance>* corpus, int rank, int size) {
  ReadFile rf(fname);
  istream& in = *rf.stream();
  string line;
  int lc = 0;
  bool flag = false;
  while(getline(in, line)) {
    ++lc;
    if ((lc-1) % size != rank) continue;
    if (rank == 0 && lc % 10 == 0) { cerr << '.' << flush; flag = true; }
    if (rank == 0 && lc % 400 == 0) { cerr << " [" << lc << "]\n"; flag = false; }
    size_t pos = line.rfind('\t');
    assert(pos != string::npos);
    corpus->push_back(TrainingInstance());
    ParseInstance(line, pos + 1, &corpus->back(), lc);
  }
  if (flag) cerr << "\nRead " << lc << " training instances\n";
}

