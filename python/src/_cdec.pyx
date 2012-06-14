from libcpp.string cimport string
from libcpp.vector cimport vector
from cython.operator cimport dereference as deref
from utils cimport *
cimport hypergraph
cimport decoder
cimport lattice
cimport kbest as kb

SetSilent(True)

class ParseFailed(Exception):
    pass

cdef class Weights:
    cdef vector[weight_t]* weights

    def __cinit__(self, Decoder decoder):
        self.weights = &decoder.dec.CurrentWeightVector()

    def __getitem__(self, char* fname):
        cdef unsigned fid = FDConvert(fname)
        if fid <= self.weights.size():
            return self.weights[0][fid]
        raise KeyError(fname)
    
    def __setitem__(self, char* fname, float value):
        cdef unsigned fid = FDConvert(<char *>fname)
        if self.weights.size() <= fid:
            self.weights.resize(fid + 1)
        self.weights[0][fid] = value

    def __iter__(self):
        cdef unsigned fid
        for fid in range(1, self.weights.size()):
            yield FDConvert(fid).c_str(), self.weights[0][fid]

cdef class Decoder:
    cdef decoder.Decoder* dec
    cdef public Weights weights

    def __cinit__(self, char* config):
        decoder.register_feature_functions()
        cdef istringstream* config_stream = new istringstream(config)
        self.dec = new decoder.Decoder(config_stream)
        del config_stream
        self.weights = Weights(self)

    def __dealloc__(self):
        del self.dec

    def read_weights(self, cfg):
        with open(cfg) as fp:
            for line in fp:
                fname, value = line.split()
                self.weights[fname.strip()] = float(value)

    # TODO: list, lattice translation
    def translate(self, unicode sentence, grammar=None):
        if grammar:
            self.dec.SetSentenceGrammarFromString(string(<char *> grammar))
        inp = sentence.strip().encode('utf8')
        cdef decoder.BasicObserver observer = decoder.BasicObserver()
        self.dec.Decode(string(<char *>inp), &observer)
        if observer.hypergraph == NULL:
            raise ParseFailed()
        cdef Hypergraph hg = Hypergraph()
        hg.hg = new hypergraph.Hypergraph(observer.hypergraph[0])
        return hg
    
cdef class Hypergraph:
    cdef hypergraph.Hypergraph* hg
    cdef MT19937* rng

    def __dealloc__(self):
        del self.hg
        if self.rng != NULL:
            del self.rng

    def viterbi(self):
        assert (self.hg != NULL)
        cdef vector[WordID] trans
        hypergraph.ViterbiESentence(self.hg[0], &trans)
        cdef str sentence = GetString(trans).c_str()
        return sentence.decode('utf8')

    def viterbi_tree(self):
        assert (self.hg != NULL)
        cdef str tree = hypergraph.ViterbiETree(self.hg[0]).c_str()
        return tree.decode('utf8')

    def kbest(self, size):
        assert (self.hg != NULL)
        cdef kb.KBestDerivations[vector[WordID], kb.ESentenceTraversal]* derivations = new kb.KBestDerivations[vector[WordID], kb.ESentenceTraversal](self.hg[0], size)
        cdef kb.KBestDerivations[vector[WordID], kb.ESentenceTraversal].Derivation* derivation
        cdef str tree
        cdef unsigned k
        for k in range(size):
            derivation = derivations.LazyKthBest(self.hg.nodes_.size() - 1, k)
            if not derivation: break
            tree = GetString(derivation._yield).c_str()
            yield tree.decode('utf8')
        del derivations

    def kbest_tree(self, size):
        assert (self.hg != NULL)
        cdef kb.KBestDerivations[vector[WordID], kb.ETreeTraversal]* derivations = new kb.KBestDerivations[vector[WordID], kb.ETreeTraversal](self.hg[0], size)
        cdef kb.KBestDerivations[vector[WordID], kb.ETreeTraversal].Derivation* derivation
        cdef str sentence
        cdef unsigned k
        for k in range(size):
            derivation = derivations.LazyKthBest(self.hg.nodes_.size() - 1, k)
            if not derivation: break
            sentence = GetString(derivation._yield).c_str()
            yield sentence.decode('utf8')
        del derivations

    def intersect(self, Lattice lat):
        assert (self.hg != NULL)
        hypergraph.Intersect(lat.lattice[0], self.hg)

    def sample(self, unsigned n):
        assert (self.hg != NULL)
        cdef vector[hypergraph.Hypothesis]* hypos = new vector[hypergraph.Hypothesis]()
        if self.rng == NULL:
            self.rng = new MT19937()
        hypergraph.sample_hypotheses(self.hg[0], n, self.rng, hypos)
        cdef str sentence
        cdef unsigned k
        for k in range(hypos.size()):
            sentence = GetString(hypos[0][k].words).c_str()
            yield sentence.decode('utf8')
        del hypos

    # TODO: get feature expectations, get partition function ("inside" score)
    # TODO: reweight the forest with different weights (Hypergraph::Reweight)
    # TODO: inside-outside pruning

cdef class Lattice:
    cdef lattice.Lattice* lattice

    def __init__(self, tuple plf_tuple):
        self.lattice = new lattice.Lattice()
        cdef bytes plf = str(plf_tuple)
        hypergraph.PLFtoLattice(string(<char *>plf), self.lattice)

    def __str__(self):
        return hypergraph.AsPLF(self.lattice[0]).c_str()

    def __iter__(self):
        return iter(eval(str(self)))

    def __dealloc__(self):
        del self.lattice

# TODO: wrap SparseVector
