#include <iostream>
#include <vector>

#include <boost/program_options.hpp>

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
        ("scale,a",po::value<double>()->default_value(1.0), "Posterior scaling factor (alpha)")
        ("evaluation_metric,m",po::value<string>()->default_value("ibm_bleu"), "Evaluation metric")
        ("input,i",po::value<string>()->default_value("-"), "File to read k-best lists from")
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

struct LossComparer {
  bool operator()(const pair<vector<WordID>, prob_t>& a, const pair<vector<WordID>, prob_t>& b) const {
    return a.second < b.second;
  }
};

bool ReadKBestList(istream* in, string* sent_id, vector<pair<vector<WordID>, prob_t> >* list) {
  static string cache_id;
  static pair<vector<WordID>, prob_t> cache_pair;
  list->clear();
  string cur_id;
  if (cache_pair.first.size() > 0) {
    list->push_back(cache_pair);
    cur_id = cache_id;
    cache_pair.first.clear();
  }
  string line;
  string tstr;
  while(*in) {
    getline(*in, line);
    if (line.empty()) continue;
    size_t p1 = line.find(" ||| ");
    if (p1 == string::npos) { cerr << "Bad format: " << line << endl; abort(); }
    size_t p2 = line.find(" ||| ", p1 + 4);
    if (p2 == string::npos) { cerr << "Bad format: " << line << endl; abort(); }
    size_t p3 = line.rfind(" ||| ");
    cache_id = line.substr(0, p1);
    tstr = line.substr(p1 + 5, p2 - p1 - 5);
    double val = strtod(line.substr(p3 + 5).c_str(), NULL);
    TD::ConvertSentence(tstr, &cache_pair.first);
    cache_pair.second.logeq(val);
    if (cur_id.empty()) cur_id = cache_id;
    if (cur_id == cache_id) {
      list->push_back(cache_pair);
      *sent_id = cur_id;
      cache_pair.first.clear();
    } else { break; }
  }
  return !list->empty();
}

int main(int argc, char** argv) {
  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);
  const string smetric = conf["evaluation_metric"].as<string>();
  EvaluationMetric* metric = EvaluationMetric::Instance(smetric);

  const bool is_loss = (UppercaseString(smetric) == "TER");
  const bool output_list = conf.count("output_list") > 0;
  const string file = conf["input"].as<string>();
  const double mbr_scale = conf["scale"].as<double>();
  cerr << "Posterior scaling factor (alpha) = " << mbr_scale << endl;

  vector<pair<vector<WordID>, prob_t> > list;
  ReadFile rf(file);
  string sent_id;
  while(ReadKBestList(rf.stream(), &sent_id, &list)) {
    vector<prob_t> joints(list.size());
    const prob_t max_score = pow(list.front().second, mbr_scale);
    prob_t marginal = prob_t::Zero();
    for (int i = 0 ; i < list.size(); ++i) {
      const prob_t joint = pow(list[i].second, mbr_scale) / max_score;
      joints[i] = joint;
      // cerr << "list[" << i << "] joint=" << log(joint) << endl;
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

