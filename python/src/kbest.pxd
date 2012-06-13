from libcpp.vector cimport vector
from utils cimport WordID
cimport hypergraph

cdef extern from "decoder/viterbi.h":
    cdef cppclass ESentenceTraversal:
        pass
    cdef cppclass ETreeTraversal:
        pass

cdef extern from "decoder/kbest.h" namespace "KBest":
    cdef cppclass KBestDerivations[T, Traversal]:
        cppclass Derivation:
            T _yield "yield"
        KBestDerivations(hypergraph.Hypergraph hg, unsigned k)
        Derivation* LazyKthBest(unsigned v, unsigned k)
