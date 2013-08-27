from libc.string cimport strrchr, strstr, strcpy, strlen
from libc.stdlib cimport malloc, realloc, strtol

cdef int INDEX_SHIFT = 3
cdef int INDEX_MASK = (1<<INDEX_SHIFT)-1

cdef class Alphabet:
    cdef readonly StringMap terminals, nonterminals
    cdef int first_nonterminal, last_nonterminal
    cdef dict id2sym

    def __cinit__(self):
        self.terminals = StringMap()
        self.nonterminals = StringMap()
        self.id2sym = {}
        self.first_nonterminal = -1

    def __dealloc__(self):
        pass

    cdef int isvar(self, int sym):
        return sym < 0

    cdef int isword(self, int sym):
        return sym >= 0

    cdef int getindex(self, int sym):
        return -sym & INDEX_MASK

    cdef int setindex(self, int sym, int ind):
        return -(-sym & ~INDEX_MASK | ind)

    cdef int clearindex(self, int sym):
        return -(-sym& ~INDEX_MASK)

    cdef int match(self, int sym1, int sym2):
        return self.clearindex(sym1) == self.clearindex(sym2);

    cdef char* tocat(self, int sym):
        return self.nonterminals.word((-sym >> INDEX_SHIFT)-1)

    cdef int fromcat(self, char *s):
        cdef int i
        i = self.nonterminals.index(s)
        if self.first_nonterminal == -1:
            self.first_nonterminal = i
        if i > self.last_nonterminal:
            self.last_nonterminal = i
        return -(i+1 << INDEX_SHIFT)

    cdef char* tostring(self, int sym):
        cdef int ind
        if self.isvar(sym):
            if sym in self.id2sym:
                return self.id2sym[sym]
            ind = self.getindex(sym)
            if ind > 0:
                self.id2sym[sym] = "[%s,%d]" % (self.tocat(sym), ind)
            else:
                self.id2sym[sym] = "[%s]" % self.tocat(sym)
            return self.id2sym[sym]
        else:
            return self.terminals.word(sym)

    cdef int fromstring(self, char *s, bint terminal):
        """Warning: this method is allowed to alter s."""
        cdef char *comma
        cdef int n
        n = strlen(s)
        cdef char *sep
        sep = strstr(s,"_SEP_")
        if n >= 3 and s[0] == c'[' and s[n-1] == c']' and sep == NULL:
            if terminal:
                s1 = "\\"+s
                return self.terminals.index(s1)
            s[n-1] = c'\0'
            s = s + 1
            comma = strrchr(s, c',')
            if comma != NULL:
                comma[0] = c'\0'
                return self.setindex(self.fromcat(s), strtol(comma+1, NULL, 10))
            else:
                return self.fromcat(s)
        else:
            return self.terminals.index(s)

cdef Alphabet ALPHABET = Alphabet()

cdef char* sym_tostring(int sym):
    return ALPHABET.tostring(sym)

cdef char* sym_tocat(int sym):
    return ALPHABET.tocat(sym)

cdef int sym_isvar(int sym):
    return ALPHABET.isvar(sym)

cdef int sym_getindex(int sym):
    return ALPHABET.getindex(sym)

cdef int sym_setindex(int sym, int id):
    return ALPHABET.setindex(sym, id)

cdef int sym_fromstring(char* string, bint terminal):
    return ALPHABET.fromstring(string, terminal)

def isvar(sym):
    return sym_isvar(sym)

def make_lattice(words):
    word_ids = (sym_fromstring(word, True) for word in words)
    return tuple(((word, None, 1), ) for word in word_ids)

def decode_lattice(lattice):
    return tuple((sym_tostring(sym), weight, dist) for (sym, weight, dist) in arc
            for arc in node for node in lattice)

def decode_sentence(lattice):
    return tuple(sym_tostring(sym) for ((sym, _, _),) in lattice)

def encode_words(words):
    return tuple(sym_fromstring(word, True) for word in words)

def decode_words(syms):
    return tuple(sym_tostring(sym) for sym in syms)