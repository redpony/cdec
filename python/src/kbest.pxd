from libcpp.vector cimport vector
from utils cimport WordID
from hypergraph cimport Hypergraph

cdef extern from "decoder/viterbi.h":
    cdef cppclass ESentenceTraversal:
        pass
    cdef cppclass ETreeTraversal:
        pass
    cdef cppclass FTreeTraversal:
        pass
    cdef cppclass FeatureVectorTraversal:
        pass

cdef extern from "decoder/kbest.h" namespace "KBest":
    cdef cppclass KBestDerivations[T, Traversal]:
        cppclass Derivation:
            T _yield "yield"
        KBestDerivations(Hypergraph& hg, unsigned k) nogil
        Derivation* LazyKthBest(unsigned v, unsigned k) nogil
