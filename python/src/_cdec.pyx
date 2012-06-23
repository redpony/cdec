from libcpp.string cimport string
from libcpp.vector cimport vector
from utils cimport *
cimport decoder

include "vectors.pxi"
include "hypergraph.pxi"
include "lattice.pxi"

SetSilent(True)

class ParseFailed(Exception):
    pass

cdef class Decoder:
    cdef decoder.Decoder* dec
    cdef public DenseVector weights

    def __cinit__(self, char* config):
        decoder.register_feature_functions()
        cdef istringstream* config_stream = new istringstream(config)
        self.dec = new decoder.Decoder(config_stream)
        del config_stream
        self.weights = DenseVector()
        self.weights.vector = &self.dec.CurrentWeightVector()

    def __dealloc__(self):
        del self.dec

    def read_weights(self, cfg):
        with open(cfg) as fp:
            for line in fp:
                fname, value = line.split()
                self.weights[fname.strip()] = float(value)

    def translate(self, sentence, grammar=None):
        if isinstance(sentence, unicode):
            inp = sentence.strip().encode('utf8')
        elif isinstance(sentence, str):
            inp = sentence.strip()
        elif isinstance(sentence, Lattice):
            inp = str(sentence) # PLF format
        else:
            raise TypeError('Cannot translate input type %s' % type(sentence))
        if grammar:
            self.dec.SetSentenceGrammarFromString(string(<char *> grammar))
        cdef decoder.BasicObserver observer = decoder.BasicObserver()
        self.dec.Decode(string(<char *>inp), &observer)
        if observer.hypergraph == NULL:
            raise ParseFailed()
        cdef Hypergraph hg = Hypergraph()
        hg.hg = new hypergraph.Hypergraph(observer.hypergraph[0])
        return hg
