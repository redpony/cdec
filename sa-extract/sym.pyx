from libc.string cimport strrchr, strstr, strcpy, strlen
from libc.stdlib cimport malloc, realloc, strtol

cdef int index_shift, index_mask, n_index
index_shift = 3
n_index = 1<<index_shift
index_mask = (1<<index_shift)-1
cdef id2sym
id2sym = {}

cdef class Alphabet:
    def __cinit__(self):
        self.terminals = cstrmap.StringMap()
        self.nonterminals = cstrmap.StringMap()

    def __init__(self):
        self.first_nonterminal = -1

    def __dealloc__(self):
        pass

    cdef int isvar(self, int sym):
        return sym < 0

    cdef int isword(self, int sym):
        return sym >= 0

    cdef int getindex(self, int sym):
        return -sym & index_mask

    cdef int setindex(self, int sym, int ind):
        return -(-sym & ~index_mask | ind)

    cdef int clearindex(self, int sym):
        return -(-sym& ~index_mask)

    cdef int match(self, int sym1, int sym2):
        return self.clearindex(sym1) == self.clearindex(sym2);

    cdef char* tocat(self, int sym):
        return self.nonterminals.word((-sym >> index_shift)-1)

    cdef int fromcat(self, char *s):
        cdef int i
        i = self.nonterminals.index(s)
        if self.first_nonterminal == -1:
            self.first_nonterminal = i
        if i > self.last_nonterminal:
            self.last_nonterminal = i
        return -(i+1 << index_shift)

    cdef char* tostring(self, int sym):
        cdef int ind
        if self.isvar(sym):
            if sym in id2sym:
                return id2sym[sym]

            ind = self.getindex(sym)
            if ind > 0:
                id2sym[sym] = "[%s,%d]" % (self.tocat(sym), ind)
            else:
                id2sym[sym] = "[%s]" % self.tocat(sym)
            return id2sym[sym]
                
        else:
            return self.terminals.word(sym)

    cdef int fromstring(self, char *s, int terminal):
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

# Expose Python functions as top-level functions for backward compatibility

alphabet = Alphabet()

cdef Alphabet calphabet
calphabet = alphabet

def isvar(int sym):
    return calphabet.isvar(sym)

def isword(int sym):
    return calphabet.isword(sym)

def getindex(int sym):
    return calphabet.getindex(sym)

def setindex(int sym, int ind):
    return calphabet.setindex(sym, ind)

def clearindex(int sym):
    return calphabet.clearindex(sym)

def match(int sym1, int sym2):
    return calphabet.match(sym1, sym2) != 0

def totag(int sym):
    return calphabet.tocat(sym)

def fromtag(s):
    s = s.upper()
    return calphabet.fromcat(s) 

def tostring(sym):
    if type(sym) is str:
        return sym
    else:
        return calphabet.tostring(sym)

cdef int bufsize
cdef char *buf
bufsize = 100
buf = <char *>malloc(bufsize)
cdef ensurebufsize(int size):
   global buf, bufsize
   if size > bufsize:
      buf = <char *>realloc(buf, size*sizeof(char))
      bufsize = size

def fromstring(s, terminal=False):
    cdef bytes bs
    cdef char* cs
    if terminal:
        return calphabet.fromstring(s, 1)
    else:
        bs = s
        cs = bs
        ensurebufsize(len(s)+1)
        strcpy(buf, cs)
        return calphabet.fromstring(buf, 0)

def nonterminals():
    cdef i
    l = []
    for i from calphabet.first_nonterminal <= i <= calphabet.last_nonterminal:
        l.append(-(i+1 << index_shift))
    return l
