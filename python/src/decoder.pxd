from libcpp.string cimport string
from libcpp.vector cimport vector
from hypergraph cimport Hypergraph
from utils cimport istream, weight_t, variables_map

cdef extern from "decoder/ff_register.h":
    void register_feature_functions()

cdef extern from "decoder/decoder.h":
    cdef cppclass SentenceMetadata:
        pass

    cdef cppclass DecoderObserver:
        DecoderObserver()

    cdef cppclass Decoder:
        Decoder(int argc, char** argv)
        Decoder(istream* config_file)
        bint Decode(string& inp, DecoderObserver* observer)

        # access this to either *read* or *write* to the decoder's last
        # weight vector (i.e., the weights of the finest past)
        vector[weight_t]& CurrentWeightVector()

        # void SetId(int id)
        variables_map& GetConf()

        # add grammar rules (currently only supported by SCFG decoders)
        # that will be used on subsequent calls to Decode. rules should be in standard
        # text format. This function does NOT read from a file.
        void SetSupplementalGrammar(string& grammar)
        void SetSentenceGrammarFromString(string& grammar_str)

cdef extern from "observer.h":
    cdef cppclass BasicObserver(DecoderObserver):
        Hypergraph* hypergraph
        BasicObserver()
