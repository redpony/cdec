cimport mteval

cdef SufficientStats as_stats(x, y):
    if isinstance(x, SufficientStats):
        return x
    elif x == 0 and isinstance(y, SufficientStats):
        stats = SufficientStats()
        stats.stats = new mteval.SufficientStats()
        stats.metric = (<SufficientStats> y).metric
        return stats

cdef class Candidate:
    cdef mteval.const_Candidate* candidate
    cdef public float score

    property words:
        def __get__(self):
            return unicode(GetString(self.candidate.ewords).c_str(), encoding='utf8')

    property fmap:
        def __get__(self):
            cdef SparseVector fmap = SparseVector.__new__(SparseVector)
            fmap.vector = new FastSparseVector[weight_t](self.candidate.fmap)
            return fmap

cdef class SufficientStats:
    cdef mteval.SufficientStats* stats
    cdef mteval.EvaluationMetric* metric

    def __dealloc__(self):
        del self.stats

    property score:
        def __get__(self):
            return self.metric.ComputeScore(self.stats[0])

    property detail:
        def __get__(self):
            return str(self.metric.DetailedScore(self.stats[0]).c_str())

    def __len__(self):
        return self.stats.size()

    def __iter__(self):
        for i in range(len(self)):
            yield self[i]

    def __getitem__(self, int index):
        if not 0 <= index < len(self):
            raise IndexError('sufficient stats vector index out of range')
        return self.stats[0][index]

    def __iadd__(SufficientStats self, SufficientStats other):
        self.stats[0] += other.stats[0]
        return self

    def __add__(x, y):
        cdef SufficientStats sx = as_stats(x, y)
        cdef SufficientStats sy = as_stats(y, x)
        cdef SufficientStats result = SufficientStats()
        result.stats = new mteval.SufficientStats(mteval.add(sx.stats[0], sy.stats[0]))
        result.metric = sx.metric
        return result

cdef class CandidateSet:
    cdef shared_ptr[mteval.SegmentEvaluator]* scorer
    cdef mteval.EvaluationMetric* metric
    cdef mteval.CandidateSet* cs

    def __cinit__(self, SegmentEvaluator evaluator):
        self.scorer = new shared_ptr[mteval.SegmentEvaluator](evaluator.scorer[0])
        self.metric = evaluator.metric
        self.cs = new mteval.CandidateSet()

    def __dealloc__(self):
        del self.scorer
        del self.cs

    def __len__(self):
        return self.cs.size()

    def __getitem__(self,int k):
        if not 0 <= k < self.cs.size():
            raise IndexError('candidate set index out of range')
        cdef Candidate candidate = Candidate()
        candidate.candidate = &self.cs[0][k]
        candidate.score = self.metric.ComputeScore(self.cs[0][k].eval_feats)
        return candidate

    def __iter__(self):
        cdef unsigned i
        for i in range(len(self)):
            yield self[i]

    def add_kbest(self, Hypergraph hypergraph, unsigned k):
        """cs.add_kbest(Hypergraph hypergraph, int k) -> Extract K-best hypotheses 
        from the hypergraph and add them to the candidate set."""
        self.cs.AddKBestCandidates(hypergraph.hg[0], k, self.scorer.get())

cdef class SegmentEvaluator:
    cdef shared_ptr[mteval.SegmentEvaluator]* scorer
    cdef mteval.EvaluationMetric* metric
    
    def __dealloc__(self):
        del self.scorer

    def evaluate(self, sentence):
        """se.evaluate(sentence) -> SufficientStats for the given hypothesis."""
        cdef vector[WordID] hyp
        cdef SufficientStats sf = SufficientStats()
        sf.metric = self.metric
        sf.stats = new mteval.SufficientStats()
        ConvertSentence(as_str(sentence.strip()), &hyp)
        self.scorer.get().Evaluate(hyp, sf.stats)
        return sf

    def candidate_set(self):
        """se.candidate_set() -> Candidate set using this segment evaluator for scoring."""
        return CandidateSet(self)

cdef class Scorer:
    cdef string* name
    cdef mteval.EvaluationMetric* metric

    def __cinit__(self, bytes name=None):
        if name:
            self.name = new string(name)
            self.metric = mteval.MetricInstance(self.name[0])

    def __dealloc__(self):
        del self.name
    
    def __call__(self, refs):
        if isinstance(refs, basestring):
            refs = [refs]
        cdef vector[vector[WordID]]* refsv = new vector[vector[WordID]]()
        cdef vector[WordID]* refv
        for ref in refs:
            refv = new vector[WordID]()
            ConvertSentence(as_str(ref.strip()), refv)
            refsv.push_back(refv[0])
            del refv
        cdef unsigned i
        cdef SegmentEvaluator evaluator = SegmentEvaluator()
        evaluator.metric = self.metric
        evaluator.scorer = new shared_ptr[mteval.SegmentEvaluator](
                self.metric.CreateSegmentEvaluator(refsv[0]))
        del refsv # in theory should not delete but store in SegmentEvaluator
        return evaluator

    def __str__(self):
        return str(self.name.c_str())

cdef float _compute_score(void* metric_, mteval.SufficientStats* stats):
    cdef Metric metric = <Metric> metric_
    cdef list ss = []
    cdef unsigned i
    for i in range(stats.size()):
        ss.append(stats[0][i])
    return metric.score(ss)

cdef void _compute_sufficient_stats(void* metric_, 
        string* hyp,
        vector[string]* refs,
        mteval.SufficientStats* out):
    cdef Metric metric = <Metric> metric_
    cdef list refs_ = []
    cdef unsigned i
    for i in range(refs.size()):
        refs_.append(str(refs[0][i].c_str()))
    cdef list ss = metric.evaluate(str(hyp.c_str()), refs_)
    out.fields.resize(len(ss))
    for i in range(len(ss)):
        out.fields[i] = ss[i]

cdef class Metric:
    cdef Scorer scorer
    def __cinit__(self):
        self.scorer = Scorer()
        cdef bytes class_name = self.__class__.__name__
        self.scorer.name = new string(class_name)
        self.scorer.metric = mteval.PyMetricInstance(self.scorer.name[0],
                <void*> self, _compute_sufficient_stats, _compute_score)

    def __call__(self, refs):
        return self.scorer(refs)

    def score(SufficientStats stats):
        return 0

    def evaluate(self, hyp, refs):
        return []

BLEU = Scorer('IBM_BLEU')
QCRI = Scorer('QCRI_BLEU')
TER = Scorer('TER')
CER = Scorer('CER')
SSK = Scorer('SSK')
