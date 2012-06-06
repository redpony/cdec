from libcpp.vector cimport vector
from utils cimport WordID

cdef extern from "decoder/lattice.h":
    cdef cppclass LatticeArc:
        WordID label
        double cost
        int dist2next
        LatticeArc()
        LatticeArc(WordID w, double c, int i)

    cdef cppclass Lattice: # (vector[vector[LatticeArc]])
        Lattice()
        Lattice(unsigned t)
        Lattice(unsigned t, vector[LatticeArc] v)
        bint IsSentence()
