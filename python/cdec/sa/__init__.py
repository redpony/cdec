from cdec.sa._sa import make_lattice, decode_lattice, decode_sentence,\
        encode_words, decode_words, isvar,\
        SuffixArray, DataArray, LCP, Precomputation, Alignment, BiLex,\
        HieroCachingRuleFactory, Sampler, Scorer
from cdec.sa.extractor import GrammarExtractor

_SA_FEATURES = []
_SA_ANNOTATORS = {}
_SA_CONFIGURE = []

def feature(fn):
    _SA_FEATURES.append(fn)
    return fn

def annotator(fn):
    _SA_ANNOTATORS[fn.__name__] = fn

def annotate(sentence):
    meta = {}
    for name, fn in _SA_ANNOTATORS.iteritems():
        meta[name] = fn(sentence)
    return meta

def configure(fn):
    _SA_CONFIGURE.append(fn)
