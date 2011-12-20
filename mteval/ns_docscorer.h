#ifndef _NS_DOC_SCORER_H_
#define _NS_DOC_SCORER_H_

#include <vector>
#include <string>
#include <boost/shared_ptr.hpp>

struct EvaluationMetric;
struct SegmentEvaluator;
class DocumentScorer {
 public:
  ~DocumentScorer();
  DocumentScorer() {  }
  DocumentScorer(const EvaluationMetric* metric,
                 const std::vector<std::string>& ref_files,
                 const std::string& src_file = "",
                 bool verbose=false) {
    Init(metric,ref_files,src_file,verbose);
  }
  void Init(const EvaluationMetric* metric,
            const std::vector<std::string>& ref_files,
            const std::string& src_file = "",
            bool verbose=false);

  int size() const { return scorers_.size(); }
  const SegmentEvaluator* operator[](size_t i) const { return scorers_[i].get(); }
 private:
  std::vector<boost::shared_ptr<SegmentEvaluator> > scorers_;
};

#endif
