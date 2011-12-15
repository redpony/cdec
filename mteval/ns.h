#ifndef _NS_H_
#define _NS_H_

#include <string>
#include <vector>
#include <map>
#include <boost/shared_ptr.hpp>
#include "wordid.h"

class EvaluationMetric;

class SufficientStats {
 public:
  SufficientStats() : evaluation_metric() {}
  explicit SufficientStats(const std::string& encoded);
  explicit SufficientStats(const EvaluationMetric* s) : evaluation_metric(s) {}
  SufficientStats(const EvaluationMetric* s, const std::vector<float>& f) :
    evaluation_metric(s), fields(f) {}

  SufficientStats& operator+=(const SufficientStats& delta) {
    if (delta.evaluation_metric) evaluation_metric = delta.evaluation_metric;
    if (fields.size() != delta.fields.size())
      fields.resize(std::max(fields.size(), delta.fields.size()));
    for (unsigned i = 0; i < delta.fields.size(); ++i)
      fields[i] += delta.fields[i];
    return *this;
  }
  SufficientStats& operator-=(const SufficientStats& delta) {
    if (delta.evaluation_metric) evaluation_metric = delta.evaluation_metric;
    if (fields.size() != delta.fields.size())
      fields.resize(std::max(fields.size(), delta.fields.size()));
    for (unsigned i = 0; i < delta.fields.size(); ++i)
      fields[i] -= delta.fields[i];
    return *this;
  }
  SufficientStats& operator*=(const double& scalar) {
    for (unsigned i = 0; i < fields.size(); ++i)
      fields[i] *= scalar;
    return *this;
  }
  SufficientStats& operator/=(const double& scalar) {
    for (unsigned i = 0; i < fields.size(); ++i)
      fields[i] /= scalar;
    return *this;
  }
  bool operator==(const SufficientStats& other) const {
    return other.fields == fields;
  }
  size_t size() const { return fields.size(); }
  float operator[](size_t i) const {
    if (i < fields.size()) return fields[i];
    return 0;
  }
  void Encode(std::string* out) const;

  const EvaluationMetric* evaluation_metric;
  std::vector<float> fields;
};

inline const SufficientStats& operator+(const SufficientStats& a, const SufficientStats& b) {
  SufficientStats res(a);
  return res += b;
}

inline const SufficientStats& operator-(const SufficientStats& a, const SufficientStats& b) {
  SufficientStats res(a);
  return res -= b;
}

struct SegmentEvaluator {
  virtual ~SegmentEvaluator();
  virtual void Evaluate(const std::vector<WordID>& hyp, SufficientStats* out) const = 0;
};

// Instructions for implementing a new metric
//   Override MetricId() and give the metric a unique string name (no spaces)
//   To Instance(), add something that creates the metric
//   Implement ONE of the following:
//      1) void ComputeSufficientStatistics(const std::vector<std::vector<WordID> >& refs, SufficientStats* out) const;
//      2) a new SegmentEvaluator class AND CreateSegmentEvaluator(const std::vector<std::vector<WordID> >& refs) const;
//   The later (#2) is only used when it is necessary to precompute per-segment data from a set of refs
//   Implement ComputeScore(const SufficientStats& stats) const;
class EvaluationMetric {
 public:
  static EvaluationMetric* Instance(const std::string& metric_id = "IBM_BLEU");

 protected:
  EvaluationMetric(const std::string& id) : name_(id) {}
  virtual ~EvaluationMetric();

 public:
  const std::string& MetricId() const { return name_; }

  virtual float ComputeScore(const SufficientStats& stats) const = 0;
  virtual boost::shared_ptr<SegmentEvaluator> CreateSegmentEvaluator(const std::vector<std::vector<WordID> >& refs) const;
  virtual void ComputeSufficientStatistics(const std::vector<WordID>& hyp,
                                           const std::vector<std::vector<WordID> >& refs,
                                           SufficientStats* out) const;

 private:
  static std::map<std::string, EvaluationMetric*> instances_;
  const std::string name_;
};

#endif

