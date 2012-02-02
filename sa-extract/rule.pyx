from libc.stdlib cimport malloc, calloc, realloc, free, strtof, strtol
from libc.string cimport strsep, strcpy, strlen

cdef extern from "strutil.h":
   char *strstrsep(char **stringp, char *delim)
   char *strip(char *s)
   char **split(char *s, char *delim, int *pn)

import sys

import sym
cimport sym
cdef sym.Alphabet alphabet
alphabet = sym.alphabet

global span_limit
span_limit = None

cdef int bufsize
cdef char *buf
bufsize = 100
buf = <char *>malloc(bufsize)
cdef ensurebufsize(int size):
   global buf, bufsize
   if size > bufsize:
      buf = <char *>realloc(buf, size*sizeof(char))
      bufsize = size
      
cdef class Phrase:
   def __cinit__(self, words):
      cdef int i, j, n, n_vars
      cdef char **toks
      cdef bytes bwords
      cdef char* cwords

      n_vars = 0
      if type(words) is str:
         ensurebufsize(len(words)+1)
         bwords = words
         cwords = bwords
         strcpy(buf, cwords)
         toks = split(buf, NULL, &n)
         self.syms = <int *>malloc(n*sizeof(int))
         for i from 0 <= i < n:
            self.syms[i] = alphabet.fromstring(toks[i], 0)
            if alphabet.isvar(self.syms[i]):
               n_vars = n_vars + 1
         
      else:
         n = len(words)
         self.syms = <int *>malloc(n*sizeof(int))
         for i from 0 <= i < n:
            self.syms[i] = words[i]
            if alphabet.isvar(self.syms[i]):
               n_vars = n_vars + 1
      self.n = n
      self.n_vars = n_vars
      self.varpos = <int *>malloc(n_vars*sizeof(int))
      j = 0
      for i from 0 <= i < n:
         if alphabet.isvar(self.syms[i]):
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
         strs.append(alphabet.tostring(s))
      return " ".join(strs)

   def instantiable(self, i, j, n):
      return span_limit is None or (j-i) <= span_limit

   def handle(self):
      """return a hashable representation that normalizes the ordering
      of the nonterminal indices"""
      norm = []
      cdef int i, j, s
      i = 1
      j = 0
      for j from 0 <= j < self.n:
         s = self.syms[j]
         if alphabet.isvar(s):
            s = alphabet.setindex(s,i)
            i = i + 1
         norm.append(s)
      return tuple(norm)

   def strhandle(self):
      strs = []
      norm = []
      cdef int i, j, s
      i = 1
      j = 0
      for j from 0 <= j < self.n:
         s = self.syms[j]
         if alphabet.isvar(s):
            s = alphabet.setindex(s,i)
            i = i + 1
         norm.append(alphabet.tostring(s))
      return " ".join(norm)

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
      l = []
      for i from 0 <= i < self.n:
         l.append(self.syms[i])
      return iter(l)

   def subst(self, start, children):
      cdef int i
      for i from 0 <= i < self.n:
         if alphabet.isvar(self.syms[i]):
            start = start + children[alphabet.getindex(self.syms[i])-1]
         else:
            start = start + (self.syms[i],)
      return start

cdef class Rule:
   def __cinit__(self, lhs, f, e, owner=None, scores=None, word_alignments=None):
      cdef int i, n
      cdef char *rest

      self.word_alignments = word_alignments
      if scores is None:
         self.cscores = NULL
         self.n_scores = 0
      else:
         n = len(scores)
         self.cscores = <float *>malloc(n*sizeof(float))
         self.n_scores = n
         for i from 0 <= i < n:
            self.cscores[i] = scores[i]

   def __init__(self, lhs, f, e, owner=None, scores=None, word_alignments=None):
      if not sym.isvar(lhs):
         sys.stderr.write("error: lhs=%d\n" % lhs)
      self.lhs = lhs
      self.f = f
      self.e = e
      self.word_alignments = word_alignments

   def __dealloc__(self):
      if self.cscores != NULL:
         free(self.cscores)

   def __str__(self):
      return self.to_line()

   def __hash__(self):
      return hash((self.lhs, self.f, self.e))

   def __cmp__(self, Rule other):
      return cmp((self.lhs, self.f, self.e, self.word_alignments), (other.lhs, other.f, other.e, self.word_alignments))

   def __iadd__(self, Rule other):
      if self.n_scores != other.n_scores:
         raise ValueError
      for i from 0 <= i < self.n_scores:
         self.cscores[i] = self.cscores[i] + other.cscores[i]
      return self

   def fmerge(self, Phrase f):
      if self.f == f:
         self.f = f
      
   def arity(self):
      return self.f.arity()

   def to_line(self):
      scorestrs = []
      for i from 0 <= i < self.n_scores:
         scorestrs.append(str(self.cscores[i]))
      fields = [alphabet.tostring(self.lhs), str(self.f), str(self.e), " ".join(scorestrs)]
      if self.word_alignments is not None:
         alignstr = []
         for i from 0 <= i < len(self.word_alignments):
            alignstr.append("%d-%d" % (self.word_alignments[i]/65536, self.word_alignments[i]%65536))
         #for s,t in self.word_alignments:
             #alignstr.append("%d-%d" % (s,t)) 
         fields.append(" ".join(alignstr))
      
      return " ||| ".join(fields)

   property scores:
      def __get__(self):
         s = [None]*self.n_scores
         for i from 0 <= i < self.n_scores:
            s[i] = self.cscores[i]
         return s

      def __set__(self, s):
         if self.cscores != NULL:
            free(self.cscores)
         self.cscores = <float *>malloc(len(s)*sizeof(float))
         self.n_scores = len(s)
         for i from 0 <= i < self.n_scores:
            self.cscores[i] = s[i]

def rule_copy(r):
   r1 = Rule(r.lhs, r.f, r.e, r.owner, r.scores)
   r1.word_alignments = r.word_alignments
   return r1

