#include "ns.h"
#include "tdict.h"

typedef float (*MetricScoreCallback)(void*, SufficientStats* stats);
typedef void (*MetricStatsCallback)(void*,
        std::string *hyp,
        std::vector<std::string> *refs,
        SufficientStats* out);

struct PythonEvaluationMetric : public EvaluationMetric {

    PythonEvaluationMetric(const std::string& id) : EvaluationMetric(id) {}

    static EvaluationMetric* Instance(const std::string& id, 
            void* obj,
            MetricStatsCallback statscb,
            MetricScoreCallback scorecb) {
        PythonEvaluationMetric* metric = new PythonEvaluationMetric(id);
        metric->pymetric = obj;
        metric->_compute_score =  scorecb;
        metric->_compute_sufficient_stats = statscb;
        return metric;
    }

    float ComputeScore(const SufficientStats& stats) const {
        SufficientStats stats_(stats);
        return _compute_score(pymetric, &stats_);
    }

    void ComputeSufficientStatistics(const std::vector<WordID>& hyp,
            const std::vector<std::vector<WordID> >& refs,
            SufficientStats* out) const {
        std::string hyp_(TD::GetString(hyp));
        std::vector<std::string> refs_;
        for(unsigned i = 0; i < refs.size(); ++i) {
            refs_.push_back(TD::GetString(refs[i]));
        }
        _compute_sufficient_stats(pymetric, &hyp_, &refs_, out);
    }

    void* pymetric;
    MetricStatsCallback _compute_sufficient_stats;
    MetricScoreCallback _compute_score;
};
