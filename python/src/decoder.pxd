from libcpp.string cimport string
from libcpp.vector cimport vector
from hypergraph cimport Hypergraph
from grammar cimport Grammar
from utils cimport *

cdef extern from "decoder/ff_register.h":
    void register_feature_functions()

cdef extern from "decoder/decoder.h":
    cdef cppclass SentenceMetadata:
        pass

    cdef cppclass DecoderObserver:
        DecoderObserver()

    cdef cppclass Decoder:
        Decoder(istream* config_file) nogil
        bint Decode(string& inp, DecoderObserver* observer) nogil

        # access this to either *read* or *write* to the decoder's last
        # weight vector (i.e., the weights of the finest past)
        vector[weight_t]& CurrentWeightVector()

        # void SetId(int id)
        variables_map& GetConf()

        # add grammar rules (currently only supported by SCFG decoders)
        void AddSupplementalGrammarFromString(string& grammar_str) nogil
        void AddSupplementalGrammar(shared_ptr[Grammar] grammar) nogil

cdef extern from "observer.h":
    cdef cppclass BasicObserver(DecoderObserver):
        Hypergraph* hypergraph
        BasicObserver()
