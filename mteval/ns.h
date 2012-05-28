#ifndef _NS_H_
#define _NS_H_

#include <string>
#include <vector>
#include <map>
#include <boost/shared_ptr.hpp>
#include "wordid.h"
#include <iostream>

class SufficientStats {
 public:
  SufficientStats() : id_() {}
  explicit SufficientStats(const std::string& encoded);
  SufficientStats(const std::string& mid, const std::vector<float>& f) :
    id_(mid), fields(f) {}

  SufficientStats& operator+=(const SufficientStats& delta) {
    if (id_.empty() && delta.id_.size()) id_ = delta.id_;
    if (fields.size() != delta.fields.size())
      fields.resize(std::max(fields.size(), delta.fields.size()));
    for (unsigned i = 0; i < delta.fields.size(); ++i)
      fields[i] += delta.fields[i];
    return *this;
  }
  SufficientStats& operator-=(const SufficientStats& delta) {
    if (id_.empty() && delta.id_.size()) id_ = delta.id_;
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
  bool IsAdditiveIdentity() const {
    for (unsigned i = 0; i < fields.size(); ++i)
      if (fields[i]) return false;
    return true;
  }
  size_t size() const { return fields.size(); }
  float operator[](size_t i) const {
    if (i < fields.size()) return fields[i];
    return 0;
  }
  void Encode(std::string* out) const;

  void swap(SufficientStats& other) {
    id_.swap(other.id_);
    fields.swap(other.fields);
  }

  std::string id_;
  std::vector<float> fields;
};

inline const SufficientStats operator+(const SufficientStats& a, const SufficientStats& b) {
  SufficientStats res(a);
  return res += b;
}

inline const SufficientStats operator-(const SufficientStats& a, const SufficientStats& b) {
  SufficientStats res(a);
  return res -= b;
}

struct SegmentEvaluator {
  virtual ~SegmentEvaluator();
  virtual void Evaluate(const std::vector<WordID>& hyp, SufficientStats* out) const = 0;
};

// Instructions for implementing a new metric
//   To Instance(), add something that creates the metric
//   Implement ComputeScore(const SufficientStats& stats) const;
//   Implement ONE of the following:
//      1) void ComputeSufficientStatistics(const std::vector<std::vector<WordID> >& refs, SufficientStats* out) const;
//      2) a new SegmentEvaluator class AND CreateSegmentEvaluator(const std::vector<std::vector<WordID> >& refs) const;
//    [The later (#2) is only used when it is necessary to precompute per-segment data from a set of refs]
//   OPTIONAL: Override SufficientStatisticsVectorSize() if it is easy to do so
class EvaluationMetric {
 public:
  static EvaluationMetric* Instance(const std::string& metric_id = "IBM_BLEU");

 protected:
  EvaluationMetric(const std::string& id) : name_(id) {}
  virtual ~EvaluationMetric();

 public:
  const std::string& MetricId() const { return name_; }

  // returns true for metrics like WER and TER where lower scores are better
  // false for metrics like BLEU and METEOR where higher scores are better
  virtual bool IsErrorMetric() const;

  virtual unsigned SufficientStatisticsVectorSize() const;
  virtual float ComputeScore(const SufficientStats& stats) const = 0;
  virtual std::string DetailedScore(const SufficientStats& stats) const;
  virtual boost::shared_ptr<SegmentEvaluator> CreateSegmentEvaluator(const std::vector<std::vector<WordID> >& refs) const;
  virtual void ComputeSufficientStatistics(const std::vector<WordID>& hyp,
                                           const std::vector<std::vector<WordID> >& refs,
                                           SufficientStats* out) const;

 private:
  static std::map<std::string, EvaluationMetric*> instances_;
  const std::string name_;
};

#endif

