from libc.stdlib cimport malloc, calloc, realloc, free, strtof, strtol
from libc.string cimport strsep, strcpy, strlen
        
cdef class Phrase:

    def __cinit__(self, words):
        cdef int i, j, n, n_vars
        n_vars = 0
        n = len(words)
        self.syms = <int *>malloc(n*sizeof(int))
        for i from 0 <= i < n:
            self.syms[i] = words[i]
            if sym_isvar(self.syms[i]):
                n_vars += 1
        self.n = n
        self.n_vars = n_vars
        self.varpos = <int *>malloc(n_vars*sizeof(int))
        j = 0
        for i from 0 <= i < n:
            if sym_isvar(self.syms[i]):
                self.varpos[j] = i
                j = j + 1

    def __dealloc__(self):
        free(self.syms)
        free(self.varpos)

    def __str__(self):
        strs = []
        cdef int i, s
        for i from 0 <= i < self.n:
            s = self.syms[i]
            strs.append(sym_tostring(s))
        return ' '.join(strs)

    def handle(self):
        """return a hashable representation that normalizes the ordering
        of the nonterminal indices"""
        norm = []
        cdef int i, j, s
        i = 1
        j = 0
        for j from 0 <= j < self.n:
            s = self.syms[j]
            if sym_isvar(s):
                s = sym_setindex(s,i)
                i = i + 1
            norm.append(s)
        return tuple(norm)

    def strhandle(self):
        norm = []
        cdef int i, j, s
        i = 1
        j = 0
        for j from 0 <= j < self.n:
            s = self.syms[j]
            if sym_isvar(s):
                s = sym_setindex(s,i)
                i = i + 1
            norm.append(sym_tostring(s))
        return ' '.join(norm)

    def arity(self):
        return self.n_vars

    def getvarpos(self, i):
        if 0 <= i < self.n_vars:
            return self.varpos[i]
        else:
            raise IndexError

    def getvar(self, i):
        if 0 <= i < self.n_vars:
            return self.syms[self.varpos[i]]
        else:
            raise IndexError

    cdef int chunkpos(self, int k):
        if k == 0:
            return 0
        else:
            return self.varpos[k-1]+1

    cdef int chunklen(self, int k):
        if self.n_vars == 0:
            return self.n
        elif k == 0:
            return self.varpos[0]
        elif k == self.n_vars:
            return self.n-self.varpos[k-1]-1
        else:
            return self.varpos[k]-self.varpos[k-1]-1

    def clen(self, k):
         return self.chunklen(k)

    def getchunk(self, ci):
        cdef int start, stop
        start = self.chunkpos(ci)
        stop = start+self.chunklen(ci)
        chunk = []
        for i from start <= i < stop:
            chunk.append(self.syms[i])
        return chunk

    def __cmp__(self, other):
        cdef Phrase otherp
        cdef int i
        otherp = other
        for i from 0 <= i < min(self.n, otherp.n):
            if self.syms[i] < otherp.syms[i]:
                return -1
            elif self.syms[i] > otherp.syms[i]:
                return 1
        if self.n < otherp.n:
            return -1
        elif self.n > otherp.n:
            return 1
        else:
            return 0

    def __hash__(self):
        cdef int i
        cdef unsigned h
        h = 0
        for i from 0 <= i < self.n:
            if self.syms[i] > 0:
                h = (h << 1) + self.syms[i]
            else:
                h = (h << 1) + -self.syms[i]
        return h

    def __len__(self):
        return self.n

    def __getitem__(self, i):
        return self.syms[i]

    def __iter__(self):
        cdef int i
        for i from 0 <= i < self.n:
            yield self.syms[i]

    def subst(self, start, children):
        cdef int i
        for i from 0 <= i < self.n:
            if sym_isvar(self.syms[i]):
                start = start + children[sym_getindex(self.syms[i])-1]
            else:
                start = start + (self.syms[i],)
        return start
    
    property words:
        def __get__(self):
            return [sym_tostring(w) for w in self if not sym_isvar(w)]

cdef class Rule:

    def __cinit__(self, int lhs, Phrase f, Phrase e, scores=None, word_alignments=None):
        if not sym_isvar(lhs): raise Exception('Invalid LHS symbol: %d' % lhs)
        self.lhs = lhs
        self.f = f
        self.e = e
        self.word_alignments = word_alignments
        self.scores = scores

    def __hash__(self):
        return hash((self.lhs, self.f, self.e))

    def __cmp__(self, Rule other):
        return cmp((self.lhs, self.f, self.e, self.word_alignments),
                (other.lhs, other.f, other.e, self.word_alignments))

    def fmerge(self, Phrase f):
        if self.f == f:
            self.f = f
        
    def arity(self):
        return self.f.arity()

    def __str__(self):
        cdef unsigned i
        fields = [sym_tostring(self.lhs), str(self.f), str(self.e), str(self.scores)]
        if self.word_alignments is not None:
            fields.append(' '.join('%d-%d' % a for a in self.alignments()))
        return ' ||| '.join(fields)

    def alignments(self):
        for point in self.word_alignments:
            yield point / ALIGNMENT_CODE, point % ALIGNMENT_CODE
