from libcpp.string cimport string
from libcpp.vector cimport vector
from utils cimport *
cimport lattice

cdef extern from "decoder/hg.h":
    cdef cppclass Hypergraph:
        cppclass Node:
            int id_
            WordID cat_
            WordID NT()
            #EdgesVector in_edges_
            #EdgesVector out_edges_
        Hypergraph(Hypergraph)
        vector[Node] nodes_

cdef extern from "decoder/viterbi.h":
    cdef prob_t ViterbiESentence(Hypergraph hg, vector[WordID]* trans)
    cdef string ViterbiETree(Hypergraph hg)

cdef extern from "decoder/hg_io.h" namespace "HypergraphIO":
    bint ReadFromJSON(istream* inp, Hypergraph* out)
    bint WriteToJSON(Hypergraph hg, bint remove_rules, ostream* out)

    #void WriteAsCFG(Hypergraph hg)
    #void WriteTarget(string base, unsigned sent_id, Hypergraph hg)

    void ReadFromPLF(string inp, Hypergraph* out, int line=*)
    string AsPLF(Hypergraph hg, bint include_global_parentheses=*)
    string AsPLF(lattice.Lattice lat, bint include_global_parentheses=*)
    void PLFtoLattice(string plf, lattice.Lattice* pl)

cdef extern from "decoder/hg_intersect.h" namespace "HG":
    bint Intersect(lattice.Lattice target, Hypergraph* hg)

cdef extern from "decoder/hg_sampler.h" namespace "HypergraphSampler":
    cdef cppclass Hypothesis:
        vector[WordID] words
        SparseVector[double] fmap
        prob_t model_score
    void sample_hypotheses(Hypergraph hg, unsigned n, MT19937* rng, vector[Hypothesis]* hypos)
