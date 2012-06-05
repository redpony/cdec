#coding: utf8
import cdec
import gzip

config = 'formalism=scfg'
weights = '../tests/system_tests/australia/weights'
grammar_file = '../tests/system_tests/australia/australia.scfg.gz'

decoder = cdec.Decoder(config)
decoder.read_weights(weights)
print dict(decoder.weights)
with gzip.open(grammar_file) as f:
    grammar = f.read()
sentence = u'澳洲 是 与 北韩 有 邦交 的 少数 国家 之一 。'
print 'Input:', sentence
forest = decoder.translate(sentence, grammar=grammar)
print 'Output:', forest.viterbi().encode('utf8')
