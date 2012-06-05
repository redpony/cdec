from libcpp.string cimport string
from libcpp.vector cimport vector
from utils cimport *
cimport hypergraph
cimport decoder

SetSilent(True)

class ParseFailed(Exception):
    pass

cdef class Weights:
    cdef vector[weight_t]* weights

    def __cinit__(self, Decoder decoder):
        self.weights = &decoder.dec.CurrentWeightVector()

    def __getitem__(self, char* fname):
        cdef unsigned fid = Convert(fname)
        if fid <= self.weights.size():
            return self.weights[0][fid]
        raise KeyError(fname)
    
    def __setitem__(self, char* fname, float value):
        cdef unsigned fid = Convert(<char *>fname)
        if self.weights.size() <= fid:
            self.weights.resize(fid + 1)
        self.weights[0][fid] = value

    def __iter__(self):
        cdef unsigned fid
        for fid in range(1, self.weights.size()):
            yield Convert(fid).c_str(), self.weights[0][fid]

cdef class Decoder:
    cdef decoder.Decoder* dec
    cdef public Weights weights

    def __cinit__(self, char* config):
        decoder.register_feature_functions()
        cdef istringstream* config_stream = new istringstream(config) # ConfigStream(kwargs)
        #cdef ReadFile* config_file = new ReadFile(string(config))
        #cdef istream* config_stream = config_file.stream()
        self.dec = new decoder.Decoder(config_stream)
        del config_stream
        #del config_file
        self.weights = Weights(self)

    def __dealloc__(self):
        del self.dec

    @classmethod
    def fromconfig(cls, ini):
        cdef dict config = {}
        with open(ini) as fp:
            for line in fp:
                line = line.strip()
                if not line or line.startswith('#'): continue
                param, value = line.split('=')
                config[param.strip()] = value.strip()
        return cls(**config)

    def read_weights(self, cfg):
        with open(cfg) as fp:
            for line in fp:
                fname, value = line.split()
                self.weights[fname.strip()] = float(value)

    def translate(self, unicode sentence, grammar=None):
        if grammar:
            self.dec.SetSentenceGrammarFromString(string(<char *> grammar))
        #sgml = '<seg grammar="%s">%s</seg>' % (grammar, sentence.encode('utf8'))
        sgml = sentence.strip().encode('utf8')
        cdef decoder.BasicObserver observer = decoder.BasicObserver()
        self.dec.Decode(string(<char *>sgml), &observer)
        if observer.hypergraph == NULL:
            raise ParseFailed()
        cdef Hypergraph hg = Hypergraph()
        hg.hg = new hypergraph.Hypergraph(observer.hypergraph[0])
        return hg
    
cdef class Hypergraph:
    cdef hypergraph.Hypergraph* hg

    def viterbi(self):
        assert (self.hg != NULL)
        cdef vector[WordID] trans
        hypergraph.ViterbiESentence(self.hg[0], &trans)
        cdef str sentence = GetString(trans).c_str()
        return sentence.decode('utf8')

"""
def params_str(params):
    return '\n'.join('%s=%s' % (param, value) for param, value in params.iteritems())

cdef istringstream* ConfigStream(dict params):
    ini = params_str(params)
    return new istringstream(<char *> ini)
"""
