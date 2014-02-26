#include <iostream>
#include <vector>

#include <boost/program_options.hpp>
#include <boost/functional/hash.hpp>
#ifndef HAVE_OLD_CPP
# include <unordered_map>
#else
# include <tr1/unordered_map>
namespace std { using std::tr1::unordered_map; }
#endif

#include "prob.h"
#include "tdict.h"
#include "ns.h"
#include "filelib.h"
#include "stringlib.h"

using namespace std;

namespace po = boost::program_options;

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("input,i",po::value<vector<string> >(), "Files to read k-best lists from")
        ("scale,a",po::value<vector<double> >(), "Posterior scaling factors (per file)")
        ("offset,b",po::value<vector<double> >(), "Log posterior offsets (per file)")
        ("evaluation_metric,m",po::value<string>()->default_value("ibm_bleu"), "Evaluation metric")
        ("output_list,L", "Show reranked list as output")
        ("help,h", "Help");
  po::options_description dcmdline_options;
  dcmdline_options.add(opts);
  po::store(parse_command_line(argc, argv, dcmdline_options), *conf);
  bool flag = false;
  if (flag || conf->count("help")) {
    cerr << dcmdline_options << endl;
    exit(1);
  }
}

struct ScoreComparer {
  bool operator()(const pair<vector<WordID>, prob_t>& a, const pair<vector<WordID>, prob_t>& b) const {
    return a.second > b.second;
  }
};

struct LossComparer {
  bool operator()(const pair<vector<WordID>, prob_t>& a, const pair<vector<WordID>, prob_t>& b) const {
    return a.second < b.second;
  }
};

bool ReadKBestList(const vector<double>& mbr_scale,
                   const vector<double>& mbr_offset,
                   const vector<ReadFile*>& rfs,
                   string* sent_id,
                   vector<pair<vector<WordID>, prob_t> >* list) {
  static string cache_id;
  pair<vector<WordID>, prob_t> tmp_pair;
  static vector<pair<vector<WordID>, prob_t> > cache_pair(rfs.size());
  list->clear();
  string cur_id;
  if (cache_pair[0].first.size() > 0) {
    for (unsigned i = 0; i < cache_pair.size(); ++i)
      list->push_back(cache_pair[i]);
    cur_id = cache_id;
    cache_pair.clear();
    cache_pair.resize(rfs.size());
  }
  string line;
  string tstr;
  for (unsigned fi = 0; fi < rfs.size(); ++fi) {
    istream& in = *rfs[fi]->stream();
    while(getline(in, line)) {
      size_t p1 = line.find(" ||| ");
      if (p1 == string::npos) { cerr << "Bad format: " << line << endl; abort(); }
      size_t p2 = line.find(" ||| ", p1 + 4);
      if (p2 == string::npos) { cerr << "Bad format: " << line << endl; abort(); }
      size_t p3 = line.rfind(" ||| ");
      cache_id = line.substr(0, p1);
      tstr = line.substr(p1 + 5, p2 - p1 - 5);
      double val = strtod(line.substr(p3 + 5).c_str(), NULL) * mbr_scale[fi] + mbr_offset[fi];
      TD::ConvertSentence(tstr, &tmp_pair.first);
      tmp_pair.second.logeq(val);
      if (cur_id.empty()) cur_id = cache_id;
      if (cur_id == cache_id) {
        list->push_back(tmp_pair);
        *sent_id = cur_id;
        tmp_pair.first.clear();
      } else {
        swap(cache_pair[fi], tmp_pair);
        break;
      }
    }
  }
  sort(list->begin(), list->end(), ScoreComparer());
  // for (unsigned i = 0; i < list->size(); ++i) {
  //  cerr << TD::GetString((*list)[i].first) << " ||| " << (*list)[i].second << endl;
  //}
  //cerr << endl;
  return !list->empty();
}

int main(int argc, char** argv) {
  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);
  const string smetric = conf["evaluation_metric"].as<string>();
  EvaluationMetric* metric = EvaluationMetric::Instance(smetric);

  const bool is_loss = (UppercaseString(smetric) == "TER");
  const bool output_list = conf.count("output_list") > 0;
  vector<string> file;
  if (conf.count("input") == 0)
    file.push_back("-");
  else
    file = conf["input"].as<vector<string> >();
  vector<double> mbr_scale;
  if (conf.count("scale")) mbr_scale = conf["scale"].as<vector<double> >();
  vector<double> mbr_offset;
  if (conf.count("offset")) mbr_offset = conf["offset"].as<vector<double> >();
  if (file.size() > mbr_scale.size()) mbr_scale.resize(file.size(), 1.0);
  if (file.size() > mbr_offset.size()) mbr_offset.resize(file.size(), 0.0);
  if (file.size() != mbr_scale.size()) {
    cerr << file.size() << " files specified but " << mbr_scale.size() << " scale factors given!\n";
    return 1;
  }
  if (file.size() != mbr_offset.size()) {
    cerr << file.size() << " files specified but " << mbr_offset.size() << " scale factors given!\n";
    return 1;
  }
  for (unsigned i = 0; i < file.size(); ++i)
    cerr << "Kbest file " << (i+1) << ": " << file[i] << "\t(scale=" << mbr_scale[i] << ", offset=" << mbr_offset[i] << ")\n";

  vector<pair<vector<WordID>, prob_t> > list;
  vector<ReadFile*> rfs(file.size());
  for (unsigned i = 0; i < file.size(); ++i)
    rfs[i] = new ReadFile(file[i]);
  string sent_id;
  while(ReadKBestList(mbr_scale, mbr_offset, rfs, &sent_id, &list)) {
    vector<prob_t> joints(list.size());
    const prob_t max_score = list.front().second;
    prob_t marginal = prob_t::Zero();
    for (int i = 0 ; i < list.size(); ++i) {
      const prob_t joint = list[i].second / max_score;
      joints[i] = joint;
      //cerr << "list[" << i << "] joint=" << log(joint) << endl;
      marginal += joint;
    }
    int mbr_idx = -1;
    vector<double> mbr_scores(output_list ? list.size() : 0);
    double mbr_loss = numeric_limits<double>::max();
    for (int i = 0 ; i < list.size(); ++i) {
      const vector<vector<WordID> > refs(1, list[i].first);
      boost::shared_ptr<SegmentEvaluator> segeval = metric->
          CreateSegmentEvaluator(refs);

      double wl_acc = 0;
      for (int j = 0; j < list.size(); ++j) {
        if (i != j) {
          SufficientStats ss;
          segeval->Evaluate(list[j].first, &ss);
          double loss = 1.0 - metric->ComputeScore(ss);
          if (is_loss) loss = 1.0 - loss;
          double weighted_loss = loss * (joints[j] / marginal).as_float();
          wl_acc += weighted_loss;
          if ((!output_list) && wl_acc > mbr_loss) break;
        }
      }
      if (output_list) mbr_scores[i] = wl_acc;
      if (wl_acc < mbr_loss) {
        mbr_loss = wl_acc;
        mbr_idx = i;
      }
    }
    // cerr << "ML translation: " << TD::GetString(list[0].first) << endl;
    cerr << "MBR Best idx: " << mbr_idx << endl;
    if (output_list) {
      for (int i = 0; i < list.size(); ++i)
        list[i].second.logeq(mbr_scores[i]);
      sort(list.begin(), list.end(), LossComparer());
      for (int i = 0; i < list.size(); ++i)
        cout << sent_id << " ||| "
             << TD::GetString(list[i].first) << " ||| "
             << log(list[i].second) << endl;
    } else {
      cout << TD::GetString(list[mbr_idx].first) << endl;
    }
  }
  return 0;
}

