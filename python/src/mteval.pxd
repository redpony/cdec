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

cdef extern from "mteval/ns.h" namespace "EvaluationMetric":
    EvaluationMetric* Instance(string& metric_id)
    EvaluationMetric* Instance() # IBM_BLEU

cdef extern from "training/candidate_set.h" namespace "training":
    cdef cppclass Candidate "const training::Candidate":
        vector[WordID] ewords
        FastSparseVector[weight_t] fmap
        SufficientStats eval_feats

    cdef cppclass CandidateSet:
        CandidateSet()
        unsigned size()
        Candidate& operator[](unsigned i)
        void ReadFromFile(string& file)
        void WriteToFile(string& file)
        void AddKBestCandidates(Hypergraph& hg,
                unsigned kbest_size,
                SegmentEvaluator* scorer)
