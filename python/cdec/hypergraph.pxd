from libcpp.string cimport string
from libcpp.vector cimport vector
from utils cimport *
from grammar cimport TRule
from lattice cimport Lattice

cdef extern from "decoder/hg.h":
    cdef cppclass EdgeMask "std::vector<bool>":
        EdgeMask(int size)
        bint& operator[](int)

    cdef cppclass HypergraphEdge "Hypergraph::Edge":
        int id_
        int head_node_ # position in hg.nodes_
        SmallVector[unsigned] tail_nodes_ # positions in hg.nodes_
        shared_ptr[TRule] rule_
        FastSparseVector[weight_t] feature_values_
        prob_t edge_prob_ # weights.dot(feature_values_)
        # typically source span:
        short int i_
        short int j_
        short int prev_i_
        short int prev_j_

    ctypedef HypergraphEdge const_HypergraphEdge "const Hypergraph::Edge"

    cdef cppclass HypergraphNode "Hypergraph::Node":
        int id_
        WordID cat_ # non-terminal category if <0, 0 if not set
        vector[int] in_edges_ # positions in hg.edge_prob_
        vector[int] out_edges_ # positions in hg.edge_prob_

    ctypedef HypergraphNode const_HypergraphNode "const Hypergraph::Node"

    cdef cppclass Hypergraph:
        Hypergraph()
        Hypergraph(Hypergraph) nogil
        vector[HypergraphNode] nodes_
        vector[HypergraphEdge] edges_
        int GoalNode()
        double NumberOfPaths()
        void Reweight(vector[weight_t]& weights) nogil
        void Reweight(FastSparseVector[weight_t]& weights) nogil
        bint PruneInsideOutside(double beam_alpha,
                                double density,
                                EdgeMask* preserve_mask,
                                bint use_sum_prod_semiring,
                                double scale,
                                bint safe_inside) nogil
        shared_ptr[Hypergraph] CreateViterbiHypergraph(EdgeMask* preserve_mask) nogil

cdef extern from "decoder/viterbi.h":
    prob_t ViterbiESentence(Hypergraph& hg, vector[WordID]* trans) nogil
    string ViterbiETree(Hypergraph& hg) nogil
    prob_t ViterbiFSentence(Hypergraph& hg, vector[WordID]* trans) nogil
    string ViterbiFTree(Hypergraph& hg) nogil
    FastSparseVector[weight_t] ViterbiFeatures(Hypergraph& hg) nogil
    FastSparseVector[weight_t] ViterbiFeatures(Hypergraph& hg, 
            FastSparseVector[weight_t]* weights,
            bint fatal_dotprod_disagreement) nogil
    string JoshuaVisualizationString(Hypergraph& hg) nogil

cdef extern from "decoder/hg_io.h" namespace "HypergraphIO":
    # Hypergraph JSON I/O
    bint ReadFromJSON(istream* inp, Hypergraph* out)
    bint WriteToJSON(Hypergraph& hg, bint remove_rules, ostream* out)
    # Hypergraph PLF I/O
    void ReadFromPLF(string& inp, Hypergraph* out)
    string AsPLF(Hypergraph& hg, bint include_global_parentheses)
    # Lattice PLF I/O
    void PLFtoLattice(string& plf, Lattice* pl)
    string AsPLF(Lattice& lat, bint include_global_parentheses)

cdef extern from "decoder/hg_intersect.h" namespace "HG":
    bint Intersect(Lattice& target, Hypergraph* hg) nogil

cdef extern from "decoder/hg_sampler.h" namespace "HypergraphSampler":
    cdef cppclass Hypothesis:
        vector[WordID] words
        FastSparseVector[weight_t] fmap
        prob_t model_score
    void sample_hypotheses(Hypergraph& hg, 
                           unsigned n, 
                           MT19937* rng, 
                           vector[Hypothesis]* hypos) nogil
    void sample_trees(Hypergraph& hg, 
                      unsigned n, 
                      MT19937* rng, 
                      vector[string]* trees) nogil

cdef extern from "decoder/csplit.h" namespace "CompoundSplit":
    int GetFullWordEdgeIndex(Hypergraph& forest)

cdef extern from "decoder/inside_outside.h":
    prob_t InsideOutside "InsideOutside<prob_t, EdgeProb, SparseVector<prob_t>, EdgeFeaturesAndProbWeightFunction>" (Hypergraph& hg, FastSparseVector[prob_t]* result) nogil
