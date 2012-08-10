#coding: utf8
import cdec
import gzip

weights = '../tests/system_tests/australia/weights'
grammar_file = '../tests/system_tests/australia/australia.scfg.gz'

# Load decoder width configuration
decoder = cdec.Decoder(formalism='scfg')
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
f_tree, e_tree = forest.viterbi_trees()
print ' FTree[0]:', f_tree.encode('utf8')
print ' ETree[0]:', e_tree.encode('utf8')
print 'LgProb[0]:', forest.viterbi_features().dot(decoder.weights)

# Get k-best translations
kbest = zip(forest.kbest(5), forest.kbest_trees(5), forest.kbest_features(5))
for i, (sentence, (f_tree, e_tree), features) in enumerate(kbest, 1):
    print 'Output[%d]:' % i, sentence.encode('utf8')
    print ' FTree[%d]:' % i, f_tree.encode('utf8')
    print ' ETree[%d]:' % i, e_tree.encode('utf8')
    print ' FVect[%d]:' % i, dict(features)

# Sample translations from the forest
for sentence in forest.sample(5):
    print 'Sample:', sentence.encode('utf8')

# Get feature vector for 1best
fsrc = forest.viterbi_features()

# Feature expectations
print 'Feature expectations:', dict(forest.inside_outside())

# Reference lattice
lattice = ((('australia',0,1),),(('is',0,1),),(('one',0,1),),(('of',0,1),),(('the',0,4),('a',0,4),('a',0,1),('the',0,1),),(('small',0,1),('tiny',0,1),('miniscule',0,1),('handful',0,2),),(('number',0,1),('group',0,1),),(('of',0,2),),(('few',0,1),),(('countries',0,1),),(('that',0,1),),(('has',0,1),('have',0,1),),(('diplomatic',0,1),),(('relations',0,1),),(('with',0,1),),(('north',0,1),),(('korea',0,1),),(('.',0,1),),)

lat = cdec.Lattice(lattice)
assert (lattice == tuple(lat))

# Intersect forest and lattice
assert forest.intersect(lat)

# Get best synchronous parse
f_tree, e_tree = forest.viterbi_trees()
print 'FTree:', f_tree.encode('utf8')
print 'ETree:', e_tree.encode('utf8')

# Compare 1best and reference feature vectors
fref = forest.viterbi_features()
print dict(fsrc - fref)

# Prune hypergraph
forest.prune(density=100)
