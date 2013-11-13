from itertools import chain
import os, sys
import cdec.configobj
from cdec.sa.features import EgivenFCoherent, SampleCountF, CountEF,\
        MaxLexEgivenF, MaxLexFgivenE, IsSingletonF, IsSingletonFE,\
        IsSupportedOnline
import cdec.sa

# maximum span of a grammar rule in TEST DATA
MAX_INITIAL_SIZE = 15

class GrammarExtractor:
    def __init__(self, config, online=False, features=None):
        if isinstance(config, basestring):
            if not os.path.exists(config):
                raise IOError('cannot read configuration from {0}'.format(config))
            config = cdec.configobj.ConfigObj(config, unrepr=True)
        alignment = cdec.sa.Alignment(from_binary=config['a_file'])
        self.factory = cdec.sa.HieroCachingRuleFactory(
                # compiled alignment object (REQUIRED)
                alignment,
                # name of generic nonterminal used by Hiero
                category="[X]",
                # maximum number of contiguous chunks of terminal symbols in RHS of a rule
                max_chunks=config['max_nt']+1,
                # maximum span of a grammar rule in TEST DATA
                max_initial_size=MAX_INITIAL_SIZE,
                # maximum number of symbols (both T and NT) allowed in a rule
                max_length=config['max_len'],
                # maximum number of nonterminals allowed in a rule (set >2 at your own risk)
                max_nonterminals=config['max_nt'],
                # maximum number of contiguous chunks of terminal symbols
                # in target-side RHS of a rule.
                max_target_chunks=config['max_nt']+1,
                # maximum number of target side symbols (both T and NT) allowed in a rule.
                max_target_length=MAX_INITIAL_SIZE,
                # minimum span of a nonterminal in the RHS of a rule in TEST DATA
                min_gap_size=1,
                # filename of file containing precomputed collocations
                precompute_file=config['precompute_file'],
                # maximum frequency rank of patterns used to compute triples (< 20)
                precompute_secondary_rank=config['rank2'],
                # maximum frequency rank of patterns used to compute collocations (< 300)
                precompute_rank=config['rank1'],
                # require extracted rules to have at least one aligned word
                require_aligned_terminal=True,
                # require each contiguous chunk of extracted rules
                # to have at least one aligned word
                require_aligned_chunks=False,
                # maximum span of a grammar rule extracted from TRAINING DATA
                train_max_initial_size=config['max_size'],
                # minimum span of an RHS nonterminal in a rule extracted from TRAINING DATA
                train_min_gap_size=config['min_gap'],
                # False if phrases should be loose (better but slower), True otherwise
                tight_phrases=config.get('tight_phrases', True),
                )

        # lexical weighting tables
        tt = cdec.sa.BiLex(from_binary=config['lex_file'])

        # TODO: clean this up
        extended_features = []
        if online:
            extended_features.append(IsSupportedOnline)
            
        # TODO: use @cdec.sa.features decorator for standard features too
        # + add a mask to disable features
        for f in cdec.sa._SA_FEATURES:
            extended_features.append(f)
            
        scorer = cdec.sa.Scorer(EgivenFCoherent, SampleCountF, CountEF, 
            MaxLexFgivenE(tt), MaxLexEgivenF(tt), IsSingletonF, IsSingletonFE,
            *extended_features)

        fsarray = cdec.sa.SuffixArray(from_binary=config['f_sa_file'])
        edarray = cdec.sa.DataArray(from_binary=config['e_file'])

        # lower=faster, higher=better; improvements level off above 200-300 range,
        # -1 = don't sample, use all data (VERY SLOW!)
        sampler = cdec.sa.Sampler(300, fsarray)

        self.factory.configure(fsarray, edarray, sampler, scorer)
        # Initialize feature definitions with configuration
        for fn in cdec.sa._SA_CONFIGURE:
            fn(config)

    def grammar(self, sentence, ctx_name=None):
        if isinstance(sentence, unicode):
            sentence = sentence.encode('utf8')
        words = tuple(chain(('<s>',), sentence.split(), ('</s>',)))
        meta = cdec.sa.annotate(words)
        cnet = cdec.sa.make_lattice(words)
        return self.factory.input(cnet, meta, ctx_name)

    # Add training instance to data
    def add_instance(self, sentence, reference, alignment, ctx_name=None):
        f_words = cdec.sa.encode_words(sentence.split())
        e_words = cdec.sa.encode_words(reference.split())
        al = sorted(tuple(int(i) for i in pair.split('-')) for pair in alignment.split())
        self.factory.add_instance(f_words, e_words, al, ctx_name)

    # Remove all incremental data for a context
    def drop_ctx(self, ctx_name=None):
        self.factory.drop_ctx(ctx_name)
