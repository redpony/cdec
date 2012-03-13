#include "ns_comb.h"

#include <iostream>

#include "stringlib.h"

using namespace std;

// e.g. COMB:IBM_BLEU=0.5;TER=0.5
CombinationMetric::CombinationMetric(const std::string& cmd) :
    EvaluationMetric(cmd),
    total_size() {
  if (cmd.find("COMB:") != 0 || cmd.size() < 9) {
    cerr << "Error in combination metric specifier: " << cmd << endl;
    exit(1);
  }
  string mix = cmd.substr(5);
  vector<string> comps;
  Tokenize(cmd.substr(5), ';', &comps);
  if(comps.size() < 2) {
    cerr << "Error in combination metric specifier: " << cmd << endl;
    exit(1);
  }
  vector<string> cwpairs;
  for (unsigned i = 0; i < comps.size(); ++i) {
    Tokenize(comps[i], '=', &cwpairs);
    if (cwpairs.size() != 2) { cerr << "Error in combination metric specifier: " << cmd << endl; exit(1); }
    metrics.push_back(EvaluationMetric::Instance(cwpairs[0]));
    coeffs.push_back(atof(cwpairs[1].c_str()));
    offsets.push_back(total_size);
    total_size += metrics.back()->SufficientStatisticsVectorSize();
    cerr << (i > 0 ? " + " : "( ") << coeffs.back() << " * " << cwpairs[0];
  }
  cerr << " )\n";
}

struct CombinationSegmentEvaluator : public SegmentEvaluator {
  CombinationSegmentEvaluator(const string& id,
                              const vector<vector<WordID> >& refs,
                              const vector<EvaluationMetric*>& metrics,
                              const vector<unsigned>& offsets,
                              const unsigned ts) : id_(id), offsets_(offsets), total_size_(ts), component_evaluators_(metrics.size()) {
    for (unsigned i = 0; i < metrics.size(); ++i)
      component_evaluators_[i] = metrics[i]->CreateSegmentEvaluator(refs);
  }
  virtual void Evaluate(const std::vector<WordID>& hyp, SufficientStats* out) const {
    out->id_ = id_;
    out->fields.resize(total_size_);
    for (unsigned i = 0; i < component_evaluators_.size(); ++i) {
      SufficientStats t;
      component_evaluators_[i]->Evaluate(hyp, &t);
      for (unsigned j = 0; j < t.fields.size(); ++j) {
        unsigned op = j + offsets_[i];
        assert(op < out->fields.size());
        out->fields[op] = t[j];
      }
    }
  }
  const string& id_;
  const vector<unsigned>& offsets_;
  const unsigned total_size_;
  vector<boost::shared_ptr<SegmentEvaluator> > component_evaluators_;
};

boost::shared_ptr<SegmentEvaluator> CombinationMetric::CreateSegmentEvaluator(const std::vector<std::vector<WordID> >& refs) const {
  boost::shared_ptr<SegmentEvaluator> res;
  res.reset(new CombinationSegmentEvaluator(MetricId(), refs, metrics, offsets, total_size));
  return res;
}

float CombinationMetric::ComputeScore(const SufficientStats& stats) const {
  float tot = 0;
  for (unsigned i = 0; i < metrics.size(); ++i) {
    SufficientStats t;
    unsigned next = total_size;
    if (i + 1 < offsets.size()) next = offsets[i+1];
    for (unsigned j = offsets[i]; j < next; ++j)
      t.fields.push_back(stats[j]);
    tot += metrics[i]->ComputeScore(t) * coeffs[i];
  }
  return tot;
}

unsigned CombinationMetric::SufficientStatisticsVectorSize() const {
  return total_size;
}

