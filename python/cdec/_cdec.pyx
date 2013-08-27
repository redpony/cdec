from libcpp.string cimport string
from libcpp.vector cimport vector
from utils cimport *
cimport decoder

cdef bytes as_str(data, char* error_msg='Cannot convert type %s to str'):
    cdef bytes ret
    if isinstance(data, unicode):
        ret = data.encode('utf8')
    elif isinstance(data, str):
        ret = data
    else:
        raise TypeError(error_msg.format(type(data)))
    return ret

include "vectors.pxi"
include "grammar.pxi"
include "hypergraph.pxi"
include "lattice.pxi"
include "mteval.pxi"

SetSilent(True)
decoder.register_feature_functions()

class InvalidConfig(Exception): pass
class ParseFailed(Exception): pass

def set_silent(yn):
    """set_silent(bool): Configure the verbosity of cdec."""
    SetSilent(yn)

def _make_config(config):
    for key, value in config.items():
        if isinstance(value, dict):
            for name, info in value.items():
                yield key, '%s %s' % (name, info)
        elif isinstance(value, list):
            for name in value:
                yield key, name
        else:
            yield key, str(value)

cdef class Decoder:
    cdef decoder.Decoder* dec
    cdef DenseVector weights

    def __init__(self, config_str=None, **config):
        """Decoder('formalism = scfg') -> initialize from configuration string
        Decoder(formalism='scfg') -> initialize from named parameters
        Create a decoder using a given configuration. Formalism is required."""
        if config_str is None:
            formalism = config.get('formalism', None)
            if formalism not in ('scfg', 'fst', 'lextrans', 'pb',
                    'csplit', 'tagger', 'lexalign'):
                raise InvalidConfig('formalism "%s" unknown' % formalism)
            config_str = '\n'.join('%s = %s' % kv for kv in _make_config(config))
        cdef istringstream* config_stream = new istringstream(config_str)
        self.dec = new decoder.Decoder(config_stream)
        del config_stream
        self.weights = DenseVector.__new__(DenseVector)
        self.weights.vector = &self.dec.CurrentWeightVector()
        self.weights.owned = True

    def __dealloc__(self):
        del self.dec

    property weights:
        def __get__(self):
            return self.weights

        def __set__(self, weights):
            if isinstance(weights, DenseVector):
                self.weights.vector[0] = (<DenseVector> weights).vector[0]
            elif isinstance(weights, SparseVector):
                ((<SparseVector> weights).vector[0]).init_vector(self.weights.vector)
            elif isinstance(weights, dict):
                self.weights.vector.clear()
                for fname, fval in weights.items():
                    self.weights[fname] = fval
            else:
                raise TypeError('cannot initialize weights with %s' % type(weights))

    property formalism:
        def __get__(self):
            cdef variables_map* conf = &self.dec.GetConf()
            return str(conf[0]['formalism'].as_str().c_str())

    def read_weights(self, weights):
        """decoder.read_weights(filename): Read decoder weights from a file."""
        with open(weights) as fp:
            for line in fp:
                if line.strip().startswith('#'): continue
                fname, value = line.split()
                self.weights[fname.strip()] = float(value)

    def translate(self, sentence, grammar=None):
        """decoder.translate(sentence, grammar=None) -> Hypergraph
        Translate a sentence (string/Lattice) with a grammar (string/list of rules)."""
        cdef bytes input_str
        if isinstance(sentence, basestring):
            input_str = as_str(sentence.strip())
        elif isinstance(sentence, Lattice):
            input_str = str(sentence) # PLF format
        else:
            raise TypeError('Cannot translate input type %s' % type(sentence))
        if grammar:
            if isinstance(grammar, basestring):
                self.dec.AddSupplementalGrammarFromString(as_str(grammar))
            else:
                self.dec.AddSupplementalGrammar(TextGrammar(grammar).grammar[0])
        cdef decoder.BasicObserver observer = decoder.BasicObserver()
        self.dec.Decode(input_str, &observer)
        if observer.hypergraph == NULL:
            raise ParseFailed()
        cdef Hypergraph hg = Hypergraph()
        hg.hg = new hypergraph.Hypergraph(observer.hypergraph[0])
        return hg
