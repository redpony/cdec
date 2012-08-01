cimport grammar
cimport cdec.sa._sa as _sa
import cdec.sa._sa as _sa

def _phrase(phrase):
    return ' '.join(w.encode('utf8') if isinstance(w, unicode) else str(w) for w in phrase)

cdef class NT:
    cdef public bytes cat
    cdef public unsigned ref
    def __init__(self, char* cat, unsigned ref=0):
        self.cat = cat
        self.ref = ref

    def __str__(self):
        if self.ref > 0:
            return '[%s,%d]' % (self.cat, self.ref)
        return '[%s]' % self.cat

cdef class NTRef:
    cdef public unsigned ref
    def __init__(self, unsigned ref):
        self.ref = ref

    def __str__(self):
        return '[%d]' % self.ref

cdef TRule convert_rule(_sa.Rule rule):
    cdef unsigned i
    cdef lhs = _sa.sym_tocat(rule.lhs)
    cdef scores = {}
    for i in range(rule.n_scores):
        scores['PhraseModel_'+str(i)] = rule.cscores[i]
    f, e = [], []
    cdef int* fsyms = rule.f.syms
    for i in range(rule.f.n):
        if _sa.sym_isvar(fsyms[i]):
            f.append(NT(_sa.sym_tocat(fsyms[i])))
        else:
            f.append(_sa.sym_tostring(fsyms[i]))
    cdef int* esyms = rule.e.syms
    for i in range(rule.e.n):
        if _sa.sym_isvar(esyms[i]):
            e.append(NTRef(_sa.sym_getindex(esyms[i])))
        else:
            e.append(_sa.sym_tostring(esyms[i]))
    cdef a = [(point/65536, point%65536) for point in rule.word_alignments]
    return TRule(lhs, f, e, scores, a)

cdef class TRule:
    cdef shared_ptr[grammar.TRule]* rule

    def __init__(self, lhs, f, e, scores, a=None):
        self.rule = new shared_ptr[grammar.TRule](new grammar.TRule())
        self.lhs = lhs
        self.e = e
        self.f = f
        self.scores = scores
        if a:
            self.a = a
        self.rule.get().ComputeArity()

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
            cdef SparseVector scores = SparseVector.__new__(SparseVector)
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

cdef class MRule(TRule):
    def __init__(self, lhs, rhs, scores, a=None):
        cdef unsigned i = 1
        e = []
        for s in rhs:
            if isinstance(s, NT):
                e.append(NTRef(i))
                i += 1
            else:
                e.append(s)
        super(MRule, self).__init__(lhs, rhs, e, scores, a)

cdef class Grammar:
    cdef shared_ptr[grammar.Grammar]* grammar
    
    def __dealloc__(self):
        del self.grammar
    
    def __iter__(self):
        cdef grammar.const_GrammarIter* root = self.grammar.get().GetRoot()
        cdef grammar.const_RuleBin* rbin = root.GetRules()
        cdef TRule trule
        cdef unsigned i
        for i in range(rbin.GetNumRules()):
            trule = TRule.__new__(TRule)
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
            if isinstance(trule, _sa.Rule):
                trule = convert_rule(trule)
            elif not isinstance(trule, TRule):
                raise ValueError('the grammar should contain TRule objects')
            _g.AddRule((<TRule> trule).rule[0])
