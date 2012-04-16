#include "dep_training.h"

#include <vector>
#include <iostream>

#include "stringlib.h"
#include "filelib.h"
#include "tdict.h"
#include "picojson.h"

using namespace std;

void TrainingInstance::ReadTraining(const string& fname, vector<TrainingInstance>* corpus, int rank, int size) {
  ReadFile rf(fname);
  istream& in = *rf.stream();
  string line;
  string err;
  int lc = 0;
  bool flag = false;
  while(getline(in, line)) {
    ++lc;
    if ((lc-1) % size != rank) continue;
    if (rank == 0 && lc % 10 == 0) { cerr << '.' << flush; flag = true; }
    if (rank == 0 && lc % 400 == 0) { cerr << " [" << lc << "]\n"; flag = false; }
    size_t pos = line.rfind('\t');
    assert(pos != string::npos);
    picojson::value obj;
    picojson::parse(obj, line.begin() + pos, line.end(), &err);
    if (err.size() > 0) { cerr << "JSON parse error in " << lc << ": " << err << endl; abort(); }
    corpus->push_back(TrainingInstance());
    TrainingInstance& cur = corpus->back();
    TaggedSentence& ts = cur.ts;
    EdgeSubset& tree = cur.tree;
    assert(obj.is<picojson::object>());
    const picojson::object& d = obj.get<picojson::object>();
    const picojson::array& ta = d.find("tokens")->second.get<picojson::array>();
    for (unsigned i = 0; i < ta.size(); ++i) {
      ts.words.push_back(TD::Convert(ta[i].get<picojson::array>()[0].get<string>()));
      ts.pos.push_back(TD::Convert(ta[i].get<picojson::array>()[1].get<string>()));
    }
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
    //cerr << TD::GetString(ts.words) << endl << TD::GetString(ts.pos) << endl << tree << endl;
  }
  if (flag) cerr << "\nRead " << lc << " training instances\n";
}

