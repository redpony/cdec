cimport grammar

def _phrase(phrase):
    return ' '.join(w.encode('utf8') if isinstance(w, unicode) else str(w) for w in phrase)

cdef class NT:
    cdef public char* cat
    cdef public unsigned ref
    def __init__(self, cat, ref=0):
        self.cat = cat
        self.ref = ref

    def __str__(self):
        if self.ref > 0:
            return '[%s,%d]' % (self.cat, self.ref)
        return '[%s]' % self.cat

cdef class NTRef:
    cdef public unsigned ref
    def __init__(self, ref):
        self.ref = ref

    def __str__(self):
        return '[%d]' % self.ref

cdef class BaseTRule:
    cdef shared_ptr[grammar.TRule]* rule

    def __dealloc__(self):
        del self.rule

    property arity:
        def __get__(self):
            return self.rule.get().arity_

    property f:
        def __get__(self):
            cdef vector[WordID]* f_ = &self.rule.get().f_
            cdef WordID w
            cdef f = []
            cdef unsigned i
            cdef int idx = 0
            for i in range(f_.size()):
                w = f_[0][i]
                if w < 0:
                    idx += 1
                    f.append(NT(TDConvert(-w), idx))
                else:
                    f.append(unicode(TDConvert(w), encoding='utf8'))
            return f

        def __set__(self, f):
            cdef vector[WordID]* f_ = &self.rule.get().f_
            f_.resize(len(f))
            cdef unsigned i
            cdef int idx = 0
            for i in range(len(f)):
                if isinstance(f[i], NT):
                    f_[0][i] = -TDConvert(<char *>f[i].cat)
                else:
                    f_[0][i] = TDConvert(<char *>as_str(f[i]))

    property e:
        def __get__(self):
            cdef vector[WordID]* e_ = &self.rule.get().e_
            cdef WordID w
            cdef e = []
            cdef unsigned i
            cdef int idx = 0
            for i in range(e_.size()):
                w = e_[0][i]
                if w < 1:
                    idx += 1
                    e.append(NTRef(1-w))
                else:
                    e.append(unicode(TDConvert(w), encoding='utf8'))
            return e

        def __set__(self, e):
            cdef vector[WordID]* e_ = &self.rule.get().e_
            e_.resize(len(e))
            cdef unsigned i
            for i in range(len(e)):
                if isinstance(e[i], NTRef):
                    e_[0][i] = 1-e[i].ref
                else:
                    e_[0][i] = TDConvert(<char *>as_str(e[i]))

    property a:
        def __get__(self):
            cdef unsigned i
            cdef vector[grammar.AlignmentPoint]* a = &self.rule.get().a_
            for i in range(a.size()):
                yield (a[0][i].s_, a[0][i].t_)

        def __set__(self, a):
            cdef vector[grammar.AlignmentPoint]* a_ = &self.rule.get().a_
            a_.resize(len(a))
            cdef unsigned i
            cdef int s, t
            for i in range(len(a)):
                s, t = a[i]
                a_[0][i] = grammar.AlignmentPoint(s, t)

    property scores:
        def __get__(self):
            cdef SparseVector scores = SparseVector()
            scores.vector = new FastSparseVector[double](self.rule.get().scores_)
            return scores

        def __set__(self, scores):
            cdef FastSparseVector[double]* scores_ = &self.rule.get().scores_
            scores_.clear()
            cdef int fid
            cdef float fval
            for fname, fval in scores.items():
                fid = FDConvert(<char *>as_str(fname))
                if fid < 0: raise KeyError(fname)
                scores_.set_value(fid, fval)

    property lhs:
        def __get__(self):
            return NT(TDConvert(-self.rule.get().lhs_))

        def __set__(self, lhs):
            if not isinstance(lhs, NT):
                lhs = NT(lhs)
            self.rule.get().lhs_ = -TDConvert(<char *>lhs.cat)

    def __str__(self):
        scores = ' '.join('%s=%s' % feat for feat in self.scores)
        return '%s ||| %s ||| %s ||| %s' % (self.lhs,
                _phrase(self.f), _phrase(self.e), scores)

cdef class TRule(BaseTRule):
    def __cinit__(self, lhs, f, e, scores, a=None):
        self.rule = new shared_ptr[grammar.TRule](new grammar.TRule())
        self.lhs = lhs
        self.e = e
        self.f = f
        self.scores = scores
        if a:
            self.a = a
        self.rule.get().ComputeArity()

cdef class Grammar:
    cdef shared_ptr[grammar.Grammar]* grammar
    
    def __dealloc__(self):
        del self.grammar
    
    def __iter__(self):
        cdef grammar.GrammarIter* root = self.grammar.get().GetRoot()
        cdef grammar.RuleBin* rbin = root.GetRules()
        cdef TRule trule
        cdef unsigned i
        for i in range(rbin.GetNumRules()):
            trule = TRule()
            trule.rule = new shared_ptr[grammar.TRule](rbin.GetIthRule(i))
            yield trule

    property name:
        def __get__(self):
            self.grammar.get().GetGrammarName().c_str()

        def __set__(self, name):
            self.grammar.get().SetGrammarName(string(<char *>name))

cdef class TextGrammar(Grammar):
    def __cinit__(self, rules):
        self.grammar = new shared_ptr[grammar.Grammar](new grammar.TextGrammar())
        cdef grammar.TextGrammar* _g = <grammar.TextGrammar*> self.grammar.get()
        for trule in rules:
            if not isinstance(trule, BaseTRule):
                raise ValueError('the grammar should contain TRule objects')
            _g.AddRule((<BaseTRule> trule).rule[0])
