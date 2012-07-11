def _phrase(phrase):
    return ' '.join('[%s,%d]' % w if isinstance(w, tuple) else w.encode('utf8') for w in phrase)

cdef class TRule:
    cdef hypergraph.TRule* rule

    property arity:
        def __get__(self):
            return self.rule.arity_

    property f:
        def __get__(self):
            cdef vector[WordID]* f = &self.rule.f_
            cdef WordID w
            cdef words = []
            cdef unsigned i
            cdef int idx = 0
            for i in range(f.size()):
                w = f[0][i]
                if w < 0:
                    idx += 1
                    words.append((TDConvert(-w), idx))
                else:
                    words.append(unicode(TDConvert(w), encoding='utf8'))
            return words

    property e:
        def __get__(self):
            cdef vector[WordID]* e = &self.rule.e_
            cdef WordID w
            cdef words = []
            cdef unsigned i
            cdef int idx = 0
            for i in range(e.size()):
                w = e[0][i]
                if w < 1:
                    idx += 1
                    words.append((TDConvert(1-w), idx))
                else:
                    words.append(unicode(TDConvert(w), encoding='utf8'))
            return words

    property scores:
        def __get__(self):
            cdef SparseVector scores = SparseVector()
            scores.vector = new FastSparseVector[double](self.rule.scores_)
            return scores

    property lhs:
        def __get__(self):
            return TDConvert(-self.rule.lhs_)

    def __str__(self):
        scores = ' '.join('%s=%s' % feat for feat in self.scores)
        return '[%s] ||| %s ||| %s ||| %s' % (self.lhs, _phrase(self.f), _phrase(self.e), scores)
