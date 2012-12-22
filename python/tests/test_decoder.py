#coding:utf8
import os
import gzip
import cdec
import unittest
from nose.tools import assert_almost_equals, assert_equal

weights = os.path.dirname(__file__)+'/../../tests/system_tests/australia/weights'
ref_weights = {'WordPenalty': -2.844814, 'LanguageModel':  1.0, 'PhraseModel_0': -1.066893, 'PhraseModel_1': -0.752247, 'PhraseModel_2': -0.589793, 'PassThrough': -20.0, 'Glue': 0}

grammar_file = os.path.dirname(__file__)+'/../../tests/system_tests/australia/australia.scfg.gz'

input_sentence = u'澳洲 是 与 北韩 有 邦交 的 少数 国家 之一 。'
ref_output_sentence = u'australia is have diplomatic relations with north korea one of the few countries .'
ref_f_tree = u'(S (S (S (S (X 澳洲 是)) (X (X 与 北韩) 有 邦交)) (X 的 少数 国家 之一)) (X 。))'
ref_e_tree = u'(S (S (S (S (X australia is)) (X have diplomatic relations (X with north korea))) (X one of the few countries)) (X .))'
ref_fvector = {'PhraseModel_2': 7.082652, 'Glue': 3.0, 'PhraseModel_0': 2.014353, 'PhraseModel_1': 8.591477}

def assert_fvector_equal(vec, ref):
    vecd = dict(vec)
    assert_equal(set(vecd.keys()), set(ref.keys()))
    for k, v in ref.items():
        assert_almost_equals(vec[k], v, 6)

class TestDecoder(unittest.TestCase):
    def setUp(self):
        self.decoder = cdec.Decoder(formalism='scfg')
        self.decoder.read_weights(weights)
        with gzip.open(grammar_file) as f:
            self.grammar = f.read()

    def test_weights(self):
        assert_fvector_equal(self.decoder.weights, ref_weights)

    def test_translate(self):
        forest = self.decoder.translate(input_sentence, grammar=self.grammar)
        assert_equal(forest.viterbi(), ref_output_sentence)
        f_tree, e_tree = forest.viterbi_trees()
        assert_equal(f_tree, ref_f_tree)
        assert_equal(e_tree, ref_e_tree)
        assert_fvector_equal(forest.viterbi_features(), ref_fvector)
