
cdef class StringMap:
    def __cinit__(self):
        self.vocab = stringmap_new()

    def __dealloc__(self):
        stringmap_delete(self.vocab)
        
    cdef char *word(self, int i):
        return stringmap_word(self.vocab, i)

    cdef int index(self, char *s):
        return stringmap_index(self.vocab, s)

