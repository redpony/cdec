cdef extern from "strmap.h":
    ctypedef struct StrMap
    StrMap* stringmap_new()
    void stringmap_delete(StrMap *vocab)
    int stringmap_index(StrMap *vocab, char *s)
    char* stringmap_word(StrMap *vocab, int i)

cdef class StringMap:
    cdef StrMap *vocab
    cdef char *word(self, int i)
    cdef int index(self, char *s)

    def __cinit__(self):
        self.vocab = stringmap_new()

    def __dealloc__(self):
        stringmap_delete(self.vocab)
        
    cdef char *word(self, int i):
        return stringmap_word(self.vocab, i)

    cdef int index(self, char *s):
        return stringmap_index(self.vocab, s)
