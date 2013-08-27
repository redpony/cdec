from libc.stdio cimport FILE

cdef class FloatList:
    cdef int size
    cdef int increment
    cdef int len
    cdef float* arr
    cdef void set(self, int i, float v)
    cdef void write_handle(self, FILE* f)
    cdef void read_handle(self, FILE* f)

cdef class IntList:
    cdef int size
    cdef int increment
    cdef int len
    cdef int* arr
    cdef void set(self, int i, int val)
    cdef void _append(self, int val)
    cdef void _extend(self, IntList other)
    cdef void _extend_arr(self, int* other, int other_len)
    cdef void _clear(self)
    cdef void write_handle(self, FILE* f)
    cdef void read_handle(self, FILE* f)

cdef class FeatureVector:
    cdef IntList names
    cdef FloatList values

cdef class Phrase:
    cdef int *syms
    cdef int n, *varpos, n_vars
    cdef public int chunkpos(self, int k)
    cdef public int chunklen(self, int k)

cdef class Rule:
    cdef int lhs
    cdef readonly Phrase f, e
    cdef FeatureVector scores
    cdef int n_scores
    cdef word_alignments

cdef char* sym_tostring(int sym)
cdef char* sym_tocat(int sym)
cdef int sym_isvar(int sym)
cdef int sym_getindex(int sym)
