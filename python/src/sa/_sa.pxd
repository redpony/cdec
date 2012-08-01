cdef class Phrase:
    cdef int *syms
    cdef int n, *varpos, n_vars
    cdef public int chunkpos(self, int k)
    cdef public int chunklen(self, int k)

cdef class Rule:
    cdef public int lhs
    cdef readonly Phrase f, e
    cdef float *cscores
    cdef int n_scores
    cdef public word_alignments

cdef char* sym_tostring(int sym)
cdef char* sym_tocat(int sym)
cdef int sym_isvar(int sym)
cdef int sym_getindex(int sym)
