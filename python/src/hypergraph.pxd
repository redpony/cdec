from libcpp.string cimport string
from libcpp.vector cimport vector
from utils cimport *
from lattice cimport Lattice

cdef extern from "decoder/hg.h":
    cdef cppclass EdgeMask "std::vector<bool>":
        EdgeMask(int size)
        bint& operator[](int)

    cdef cppclass Hypergraph:
        cppclass Edge:
            int id_
            int head_node_ # position in hg.nodes_
            SmallVector[unsigned] tail_nodes_ # positions in hg.nodes_
            #TRulePtr rule_
            FastSparseVector[weight_t] feature_values_
            LogVal[double] edge_prob_
            short int i_
            short int j_
            short int prev_i_
            short int prev_j_
        cppclass Node:
            int id_
            WordID cat_
            WordID NT()
            vector[Edge] in_edges_
            vector[Edge] out_edges_
        Hypergraph(Hypergraph)
        vector[Node] nodes_
        vector[Edge] edges_
        void Reweight(vector[weight_t]& weights)
        void Reweight(FastSparseVector& weights)
        bint PruneInsideOutside(double beam_alpha,
                                double density,
                                EdgeMask* preserve_mask,
                                bint use_sum_prod_semiring,
                                double scale,
                                bint safe_inside)

cdef extern from "decoder/viterbi.h":
    LogVal[double] ViterbiESentence(Hypergraph& hg, vector[WordID]* trans)
    string ViterbiETree(Hypergraph& hg)
    LogVal[double] ViterbiFSentence(Hypergraph& hg, vector[WordID]* trans)
    string ViterbiFTree(Hypergraph& hg)
    FastSparseVector[weight_t] ViterbiFeatures(Hypergraph& hg)
    FastSparseVector[weight_t] ViterbiFeatures(Hypergraph& hg, 
            FastSparseVector[weight_t]* weights,
            bint fatal_dotprod_disagreement)

cdef extern from "decoder/hg_io.h" namespace "HypergraphIO":
    bint ReadFromJSON(istream* inp, Hypergraph* out)
    bint WriteToJSON(Hypergraph& hg, bint remove_rules, ostream* out)
    void ReadFromPLF(string& inp, Hypergraph* out, int line)
    string AsPLF(Hypergraph& hg, bint include_global_parentheses)
    void PLFtoLattice(string& plf, Lattice* pl)
    string AsPLF(Lattice& lat, bint include_global_parentheses)

cdef extern from "decoder/hg_intersect.h" namespace "HG":
    bint Intersect(Lattice& target, Hypergraph* hg)

cdef extern from "decoder/hg_sampler.h" namespace "HypergraphSampler":
    cdef cppclass Hypothesis:
        vector[WordID] words
        FastSparseVector[double] fmap
        LogVal[double] model_score
    void sample_hypotheses(Hypergraph& hg, 
                           unsigned n, 
                           MT19937* rng, 
                           vector[Hypothesis]* hypos)

cdef extern from "decoder/csplit.h" namespace "CompoundSplit":
    int GetFullWordEdgeIndex(Hypergraph& forest)
