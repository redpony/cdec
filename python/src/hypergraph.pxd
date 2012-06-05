from libcpp.string cimport string
from libcpp.vector cimport vector
from utils cimport WordID

cdef extern from "decoder/hg.h":
    cdef cppclass Hypergraph:
        Hypergraph(Hypergraph)

cdef extern from "decoder/viterbi.h":
    cdef string ViterbiESentence(Hypergraph hg, vector[WordID]* trans)
