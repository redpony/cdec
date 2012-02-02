cimport cstrmap

cdef class Alphabet:
    cdef readonly cstrmap.StringMap terminals, nonterminals
    cdef int first_nonterminal, last_nonterminal
    cdef int isvar(self, int sym)
    cdef int isword(self, int sym)
    cdef int getindex(self, int sym)
    cdef int setindex(self, int sym, int ind)
    cdef int clearindex(self, int sym)
    cdef int match(self, int sym1, int sym2)
    cdef char* tocat(self, int sym)
    cdef int fromcat(self, char *s)
    cdef char* tostring(self, int sym)
    cdef int fromstring(self, char *s, int terminal)

    
