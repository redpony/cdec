from cdec.sa._sa import make_lattice, decode_lattice, decode_sentence,\
        SuffixArray, DataArray, LCP, Precomputation, Alignment, BiLex,\
        HieroCachingRuleFactory, Sampler, Scorer
from cdec.sa.extractor import GrammarExtractor

_SA_FEATURES = []

def feature(fn):
    _SA_FEATURES.append(fn)
    return fn
