from libcpp.vector cimport vector
from libcpp.string cimport string
from utils cimport WordID

cdef extern from "decoder/lattice.h":
    cdef cppclass LatticeArc:
        WordID label
        double cost
        int dist2next
        LatticeArc()
        LatticeArc(WordID w, double c, int i)

    cdef cppclass Lattice(vector): # (vector[vector[LatticeArc]])
        Lattice()
        Lattice(unsigned t)
        Lattice(unsigned t, vector[LatticeArc]& v)
        bint IsSentence()
        vector[LatticeArc]& operator[](unsigned)

cdef extern from "decoder/lattice.h" namespace "LatticeTools":
    void ConvertTextToLattice(string& text, Lattice* pl)
