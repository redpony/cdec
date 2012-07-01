#coding: utf8
import cdec
import gzip

config = 'formalism=scfg'
weights = '../tests/system_tests/australia/weights'
grammar_file = '../tests/system_tests/australia/australia.scfg.gz'

# Load decoder width configuration
decoder = cdec.Decoder(config)
# Read weights
decoder.read_weights(weights)

print dict(decoder.weights)

# Read grammar
with gzip.open(grammar_file) as f:
    grammar = f.read()

# Input sentence
sentence = u'澳洲 是 与 北韩 有 邦交 的 少数 国家 之一 。'
print '    Input:', sentence.encode('utf8')

# Decode
forest = decoder.translate(sentence, grammar=grammar)

# Get viterbi translation
print 'Output[0]:', forest.viterbi().encode('utf8')
print ' ETree[0]:', forest.viterbi_tree().encode('utf8')
print ' FTree[0]:', forest.viterbi_source_tree().encode('utf8')
print 'LgProb[0]:', forest.viterbi_features().dot(decoder.weights)

# Get k-best translations
kbest = zip(forest.kbest(5), forest.kbest_tree(5), forest.kbest_features(5))
for i, (sentence, tree, features) in enumerate(kbest, 1):
    print 'Output[%d]:' % i, sentence.encode('utf8')
    print '  Tree[%d]:' % i, tree.encode('utf8')
    print ' FVect[%d]:' % i, dict(features)

# Sample translations from the forest
for sentence in forest.sample(5):
    print 'Sample:', sentence.encode('utf8')

# Get feature vector for 1best
fsrc = forest.viterbi_features()

# Reference lattice
lattice = ((('australia',0,1),),(('is',0,1),),(('one',0,1),),(('of',0,1),),(('the',0,4),('a',0,4),('a',0,1),('the',0,1),),(('small',0,1),('tiny',0,1),('miniscule',0,1),('handful',0,2),),(('number',0,1),('group',0,1),),(('of',0,2),),(('few',0,1),),(('countries',0,1),),(('that',0,1),),(('has',0,1),('have',0,1),),(('diplomatic',0,1),),(('relations',0,1),),(('with',0,1),),(('north',0,1),),(('korea',0,1),),(('.',0,1),),)

lat = cdec.Lattice(lattice)
assert (lattice == tuple(lat))

# Intersect forest and lattice
assert forest.intersect(lat)

# Get best synchronous parse
print forest.viterbi_tree()
print forest.viterbi_source_tree()

# Compare 1best and reference feature vectors
fref = forest.viterbi_features()
print dict(fsrc - fref)
