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

