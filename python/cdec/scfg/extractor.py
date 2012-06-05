#!/usr/bin/env python
import StringIO

import clex
import rulefactory
import calignment
import csuf
import cdat
import sym
import log

log.level = -1

from features import EgivenFCoherent, SampleCountF, CountEF,\
        MaxLexEgivenF, MaxLexFgivenE, IsSingletonF, IsSingletonFE
from features import contextless

class Output(StringIO.StringIO):
    def close(self):
        pass

    def __str__(self):
        return self.getvalue()

from itertools import chain

def get_cn(sentence):
    sentence = chain(('<s>',), sentence.split(), ('</s>',))
    sentence = (sym.fromstring(word, terminal=True) for word in sentence)
    return tuple(((word, None, 1), ) for word in sentence)

class PhonyGrammar:
    def add(self, thing):
        pass

class GrammarExtractor:
    def __init__(self, config):
        alignment = calignment.Alignment(config['a_file'], from_binary=True)
        self.factory = rulefactory.HieroCachingRuleFactory(
                # compiled alignment object (REQUIRED)
                alignment=alignment,
                # name of generic nonterminal used by Hiero
                category="[X]",
                # do not change for extraction
                grammar=PhonyGrammar(), # TODO: set to None?
                # maximum number of contiguous chunks of terminal symbols in RHS of a rule. If None, defaults to max_nonterminals+1
                max_chunks=None,
                # maximum span of a grammar rule in TEST DATA
                max_initial_size=15,
                # maximum number of symbols (both T and NT) allowed in a rule
                max_length=config['max_len'],
                # maximum number of nonterminals allowed in a rule (set >2 at your own risk)
                max_nonterminals=config['max_nt'],
                # maximum number of contiguous chunks of terminal symbols in target-side RHS of a rule. If None, defaults to max_nonterminals+1
                max_target_chunks=None,
                # maximum number of target side symbols (both T and NT) allowed in a rule. If None, defaults to max_initial_size
                max_target_length=None,
                # minimum span of a nonterminal in the RHS of a rule in TEST DATA
                min_gap_size=1,
                # filename of file containing precomputed collocations
                precompute_file=config['precompute_file'],
                # maximum frequency rank of patterns used to compute triples (don't set higher than 20).
                precompute_secondary_rank=config['rank2'],
                # maximum frequency rank of patterns used to compute collocations (no need to set higher than maybe 200-300)
                precompute_rank=config['rank1'],
                # require extracted rules to have at least one aligned word
                require_aligned_terminal=True,
                # require each contiguous chunk of extracted rules to have at least one aligned word
                require_aligned_chunks=False,
                # generate a complete grammar for each input sentence
                per_sentence_grammar=True,
                # maximum span of a grammar rule extracted from TRAINING DATA
                train_max_initial_size=config['max_size'],
                # minimum span of an RHS nonterminal in a rule extracted from TRAINING DATA
                train_min_gap_size=config['min_gap'],
                # True if phrases should be tight, False otherwise (False seems to give better results but is slower)
                tight_phrases=True,
                )
        self.fsarray = csuf.SuffixArray(config['f_sa_file'], from_binary=True)
        self.edarray = cdat.DataArray(config['e_file'], from_binary=True)

        self.factory.registerContext(self)

        # lower=faster, higher=better; improvements level off above 200-300 range, -1 = don't sample, use all data (VERY SLOW!)
        self.sampler = rulefactory.Sampler(300)
        self.sampler.registerContext(self)

        # lexical weighting tables
        tt = clex.CLex(config['lex_file'], from_binary=True)

        self.models = (EgivenFCoherent, SampleCountF, CountEF, 
                MaxLexFgivenE(tt), MaxLexEgivenF(tt), IsSingletonF, IsSingletonFE)
        self.models = tuple(contextless(feature) for feature in self.models)

    def grammar(self, sentence):
        out = Output()
        cn = get_cn(sentence)
        self.factory.input_file(cn, out)
        return str(out)

def main(config):
    sys.path.append(os.path.dirname(config))
    module =  __import__(os.path.basename(config).replace('.py', ''))
    extractor = GrammarExtractor(module.__dict__)
    print extractor.grammar(next(sys.stdin))

if __name__ == '__main__':
    import sys, os
    if len(sys.argv) != 2 or not sys.argv[1].endswith('.py'):
        sys.stderr.write('Usage: %s config.py\n' % sys.argv[0])
        sys.exit(1)
    main(*sys.argv[1:])
