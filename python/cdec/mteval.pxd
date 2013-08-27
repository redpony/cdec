from libcpp.vector cimport vector
from libcpp.string cimport string
from utils cimport *
from hypergraph cimport Hypergraph

cdef extern from "mteval/ns.h":
    cdef cppclass SufficientStats:
        SufficientStats()
        SufficientStats(SufficientStats&)
        unsigned size()
        float operator[](unsigned i)
        void swap(SufficientStats& other)
        vector[float] fields

    SufficientStats add "operator+" (SufficientStats&, SufficientStats&)

    cdef cppclass SegmentEvaluator:
        void Evaluate(vector[WordID]& hyp, SufficientStats* out)

    cdef cppclass EvaluationMetric:
        string& MetricId()
        bint IsErrorMetric()
        float ComputeScore(SufficientStats& stats)
        string DetailedScore(SufficientStats& stats)
        shared_ptr[SegmentEvaluator] CreateSegmentEvaluator(vector[vector[WordID]]& refs)
        ComputeSufficientStatistics(vector[WordID]& hyp,
                                    vector[WordID]& refs,
                                    SufficientStats* out)

    cdef EvaluationMetric* MetricInstance "EvaluationMetric::Instance" (string& metric_id)

cdef extern from "py_scorer.h":
    ctypedef float (*MetricScoreCallback)(void*, SufficientStats* stats)
    ctypedef void (*MetricStatsCallback)(void*, 
            string* hyp, vector[string]* refs, SufficientStats* out)
    
    cdef EvaluationMetric* PyMetricInstance "PythonEvaluationMetric::Instance"(
            string& metric_id, void*, MetricStatsCallback, MetricScoreCallback)

cdef extern from "training/utils/candidate_set.h" namespace "training":
    cdef cppclass Candidate:
        vector[WordID] ewords
        FastSparseVector[weight_t] fmap
        SufficientStats eval_feats

    ctypedef Candidate const_Candidate "const training::Candidate"

    cdef cppclass CandidateSet:
        CandidateSet()
        unsigned size()
        const_Candidate& operator[](unsigned i)
        void ReadFromFile(string& file)
        void WriteToFile(string& file)
        void AddKBestCandidates(Hypergraph& hg,
                unsigned kbest_size,
                SegmentEvaluator* scorer)
