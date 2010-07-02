import sys
import scipy.optimize
from scipy.stats import geom
from numpy import *
from numpy.random import random, seed

style = sys.argv[1]
if len(sys.argv) >= 3:
     seed(int(sys.argv[2]))

#
# Step 1: load the concordance counts
# 

edges = []
word_types = {}
phrase_types = {}
context_types = {}

for line in sys.stdin:
    phrase, rest = line.strip().split('\t')
    ptoks = tuple(map(lambda t: word_types.setdefault(t, len(word_types)), phrase.split()))
    pid = phrase_types.setdefault(ptoks, len(phrase_types))

    parts = rest.split('|||')
    for i in range(0, len(parts), 2):
        context, count = parts[i:i+2]

        ctx = filter(lambda x: x != '<PHRASE>', context.split())
        ctoks = tuple(map(lambda t: word_types.setdefault(t, len(word_types)), ctx))
        cid = context_types.setdefault(ctoks, len(context_types))

        cnt = int(count.strip()[2:])
        edges.append((pid, cid, cnt))

word_type_list = [None] * len(word_types)
for typ, index in word_types.items():
    word_type_list[index] = typ

phrase_type_list = [None] * len(phrase_types)
for typ, index in phrase_types.items():
    phrase_type_list[index] = typ

context_type_list = [None] * len(context_types)
for typ, index in context_types.items():
    context_type_list[index] = typ

num_tags = 5
num_types = len(word_types)
num_phrases = len(phrase_types)
num_contexts = len(context_types)
num_edges = len(edges)

print 'Read in', num_edges, 'edges', num_phrases, 'phrases', num_contexts, 'contexts and', num_types, 'word types'

#
# Step 2:  expectation maximisation 
#

def normalise(a):
    return a / float(sum(a))

class PhraseToContextModel:
    def __init__(self):
        # Pr(tag | phrase)
        self.tagDist = [normalise(random(num_tags)+1) for p in range(num_phrases)]
        # Pr(context at pos i = w | tag) indexed by i, tag, word
        self.contextWordDist = [[normalise(random(num_types)+1) for t in range(num_tags)] for i in range(4)]

    def prob(self, pid, cid):
        # return distribution p(tag, context | phrase) as vector of length |tags|
        context = context_type_list[cid]
        dist = zeros(num_tags)
        for t in range(num_tags):
            prob = self.tagDist[pid][t]
            for k, tokid in enumerate(context):
                prob *= self.contextWordDist[k][t][tokid]
            dist[t] = prob
        return dist

    def expectation_maximisation_step(self, lamba=None):
        tagCounts = zeros((num_phrases, num_tags))
        contextWordCounts = zeros((4, num_tags, num_types))

        # E-step
        llh = 0
        for pid, cid, cnt in edges:
            q = self.prob(pid, cid)
            z = sum(q)
            q /= z
            llh += log(z)
            context = context_type_list[cid]
            if lamba != None:
                q *= exp(lamba)
                q /= sum(q)
            for t in range(num_tags):
                tagCounts[pid][t] += cnt * q[t]
            for i in range(4):
                for t in range(num_tags):
                    contextWordCounts[i][t][context[i]] += cnt * q[t]

        # M-step
        for p in range(num_phrases):
            self.tagDist[p] = normalise(tagCounts[p])
        for i in range(4):
            for t in range(num_tags):
                self.contextWordDist[i][t] = normalise(contextWordCounts[i,t])

        return llh

class ContextToPhraseModel:
    def __init__(self):
        # Pr(tag | context) = Multinomial
        self.tagDist = [normalise(random(num_tags)+1) for p in range(num_contexts)]
        # Pr(phrase = w | tag) = Multinomial
        self.phraseSingleDist = [normalise(random(num_types)+1) for t in range(num_tags)]
        # Pr(phrase_1 = w | tag) = Multinomial
        self.phraseLeftDist = [normalise(random(num_types)+1) for t in range(num_tags)]
        # Pr(phrase_-1 = w | tag) = Multinomial
        self.phraseRightDist = [normalise(random(num_types)+1) for t in range(num_tags)]
        # Pr(|phrase| = l | tag) = Geometric
        self.phraseLengthDist = [0.5] * num_tags
        # n.b. internal words for phrases of length >= 3 are drawn from uniform distribution

    def prob(self, pid, cid):
        # return distribution p(tag, phrase | context) as vector of length |tags|
        phrase = phrase_type_list[pid]
        dist = zeros(num_tags)
        for t in range(num_tags):
            prob = self.tagDist[cid][t]
            f = self.phraseLengthDist[t]
            prob *= geom.pmf(len(phrase), f)
            if len(phrase) == 1:
                prob *= self.phraseSingleDist[t][phrase[0]]
            else:
                prob *= self.phraseLeftDist[t][phrase[0]]
                prob *= self.phraseRightDist[t][phrase[-1]]
            dist[t] = prob
        return dist

    def expectation_maximisation_step(self, lamba=None):
        tagCounts = zeros((num_contexts, num_tags))
        phraseSingleCounts = zeros((num_tags, num_types))
        phraseLeftCounts = zeros((num_tags, num_types))
        phraseRightCounts = zeros((num_tags, num_types))
        phraseLength = zeros(num_types)

        # E-step
        llh = 0
        for pid, cid, cnt in edges:
            q = self.prob(pid, cid)
            z = sum(q)
            q /= z
            llh += log(z)
            if lamba != None:
                q *= exp(lamba)
                q /= sum(q)
            #print 'p', phrase_type_list[pid], 'c', context_type_list[cid], 'q', q
            phrase = phrase_type_list[pid]
            for t in range(num_tags):
                tagCounts[cid][t] += cnt * q[t]
                phraseLength[t] += cnt * len(phrase) * q[t]
                if len(phrase) == 1:
                    phraseSingleCounts[t][phrase[0]] += cnt * q[t]
                else:
                    phraseLeftCounts[t][phrase[0]] += cnt * q[t]
                    phraseRightCounts[t][phrase[-1]] += cnt * q[t]

        # M-step
        for t in range(num_tags):
            self.phraseLengthDist[t] = min(max(sum(tagCounts[:,t]) / phraseLength[t], 1e-6), 1-1e-6)
            self.phraseSingleDist[t] = normalise(phraseSingleCounts[t])
            self.phraseLeftDist[t] = normalise(phraseLeftCounts[t])
            self.phraseRightDist[t] = normalise(phraseRightCounts[t])
        for c in range(num_contexts):
            self.tagDist[c] = normalise(tagCounts[c])

        #print 't', self.tagDist
        #print 'l', self.phraseLengthDist
        #print 's', self.phraseSingleDist
        #print 'L', self.phraseLeftDist
        #print 'R', self.phraseRightDist

        return llh

class ProductModel:
    """
    WARNING: I haven't verified the maths behind this model. It's quite likely to be incorrect.
    """

    def __init__(self):
        self.pcm = PhraseToContextModel()
        self.cpm = ContextToPhraseModel()

    def prob(self, pid, cid):
        p1 = self.pcm.prob(pid, cid)
        p2 = self.cpm.prob(pid, cid)
        return (p1 / sum(p1)) * (p2 / sum(p2))

    def expectation_maximisation_step(self):
        tagCountsGivenPhrase = zeros((num_phrases, num_tags))
        contextWordCounts = zeros((4, num_tags, num_types))

        tagCountsGivenContext = zeros((num_contexts, num_tags))
        phraseSingleCounts = zeros((num_tags, num_types))
        phraseLeftCounts = zeros((num_tags, num_types))
        phraseRightCounts = zeros((num_tags, num_types))
        phraseLength = zeros(num_types)

        kl = llh1 = llh2 = 0
        for pid, cid, cnt in edges:
            p1 = self.pcm.prob(pid, cid)
            llh1 += log(sum(p1)) * cnt
            p2 = self.cpm.prob(pid, cid)
            llh2 += log(sum(p2)) * cnt

            q = (p1 / sum(p1)) * (p2 / sum(p2))
            kl += log(sum(q)) * cnt
            qi = sqrt(q)
            qi /= sum(qi)

            phrase = phrase_type_list[pid]
            context = context_type_list[cid]
            for t in range(num_tags):
                tagCountsGivenPhrase[pid][t] += cnt * qi[t]
                tagCountsGivenContext[cid][t] += cnt * qi[t]
                phraseLength[t] += cnt * len(phrase) * qi[t]
                if len(phrase) == 1:
                    phraseSingleCounts[t][phrase[0]] += cnt * qi[t]
                else:
                    phraseLeftCounts[t][phrase[0]] += cnt * qi[t]
                    phraseRightCounts[t][phrase[-1]] += cnt * qi[t]
                for i in range(4):
                    contextWordCounts[i][t][context[i]] += cnt * qi[t]

        kl *= -2

        for t in range(num_tags):
            for i in range(4):
                self.pcm.contextWordDist[i][t] = normalise(contextWordCounts[i,t])
            self.cpm.phraseLengthDist[t] = min(max(sum(tagCountsGivenContext[:,t]) / phraseLength[t], 1e-6), 1-1e-6)
            self.cpm.phraseSingleDist[t] = normalise(phraseSingleCounts[t])
            self.cpm.phraseLeftDist[t] = normalise(phraseLeftCounts[t])
            self.cpm.phraseRightDist[t] = normalise(phraseRightCounts[t])
        for p in range(num_phrases):
            self.pcm.tagDist[p] = normalise(tagCountsGivenPhrase[p])
        for c in range(num_contexts):
            self.cpm.tagDist[c] = normalise(tagCountsGivenContext[c])

        # return the overall objective
        return llh1 + llh2 + kl

class RegularisedProductModel:
    # as above, but with a slack regularisation term which kills the
    # closed-form solution for the E-step

    def __init__(self, epsilon):
        self.pcm = PhraseToContextModel()
        self.cpm = ContextToPhraseModel()
        self.epsilon = epsilon
        self.lamba = zeros(num_tags)

    def prob(self, pid, cid):
        p1 = self.pcm.prob(pid, cid)
        p2 = self.cpm.prob(pid, cid)
        return (p1 / sum(p1)) * (p2 / sum(p2))

    def dual(self, lamba):
        return self.logz(lamba) + self.epsilon * dot(lamba, lamba) ** 0.5

    def dual_gradient(self, lamba):
        return self.expected_features(lamba) + self.epsilon * 2 * lamba

    def expectation_maximisation_step(self):
        # PR-step: optimise lambda to minimise log(z_lambda) + eps ||lambda||_2
        self.lamba = scipy.optimize.fmin_slsqp(self.dual, self.lamba,
                                bounds=[(0, 1e100)] * num_tags,
                                fprime=self.dual_gradient, iprint=1)

        # E,M-steps: collect expected counts under q_lambda and normalise
        llh1 = self.pcm.expectation_maximisation_step(self.lamba)
        llh2 = self.cpm.expectation_maximisation_step(-self.lamba)

        # return the overall objective: llh - KL(q||p1.p2)
        # llh = llh1 + llh2
        # kl = sum q log q / p1 p2 = sum q { lambda . phi } - log Z
        return llh1 + llh2 + self.logz(self.lamba) \
            - dot(self.lamba, self.expected_features(self.lamba))

    def logz(self, lamba):
        lz = 0
        for pid, cid, cnt in edges:
            p1 = self.pcm.prob(pid, cid)
            z1 = dot(p1 / sum(p1), exp(lamba))
            lz += log(z1) * cnt

            p2 = self.cpm.prob(pid, cid)
            z2 = dot(p2 / sum(p2), exp(-lamba))
            lz += log(z2) * cnt
        return lz

    def expected_features(self, lamba):
        fs = zeros(num_tags)
        for pid, cid, cnt in edges:
            p1 = self.pcm.prob(pid, cid)
            q1 = (p1 / sum(p1)) * exp(lamba)
            fs += cnt * q1 / sum(q1)

            p2 = self.cpm.prob(pid, cid)
            q2 = (p2 / sum(p2)) * exp(-lamba)
            fs -= cnt * q2 / sum(q2)
        return fs


class InterpolatedModel:
    def __init__(self, epsilon):
        self.pcm = PhraseToContextModel()
        self.cpm = ContextToPhraseModel()
        self.epsilon = epsilon
        self.lamba = zeros(num_tags)

    def prob(self, pid, cid):
        p1 = self.pcm.prob(pid, cid)
        p2 = self.cpm.prob(pid, cid)
        return (p1 + p2) / 2

    def dual(self, lamba):
        return self.logz(lamba) + self.epsilon * dot(lamba, lamba) ** 0.5

    def dual_gradient(self, lamba):
        return self.expected_features(lamba) + self.epsilon * 2 * lamba

    def expectation_maximisation_step(self):
        # PR-step: optimise lambda to minimise log(z_lambda) + eps ||lambda||_2
        self.lamba = scipy.optimize.fmin_slsqp(self.dual, self.lamba,
                                bounds=[(0, 1e100)] * num_tags,
                                fprime=self.dual_gradient, iprint=2)

        # E,M-steps: collect expected counts under q_lambda and normalise
        llh1 = self.pcm.expectation_maximisation_step(self.lamba)
        llh2 = self.cpm.expectation_maximisation_step(self.lamba)

        # return the overall objective: llh1 + llh2 - KL(q||p1.p2)
        # kl = sum_y q log q / 0.5 * (p1 + p2) = sum_y q(y) { -lambda . phi(y) } - log Z
        #    = -log Z + lambda . (E_q1[-phi] + E_q2[-phi]) / 2
        kl = -self.logz(self.lamba) + dot(self.lamba, self.expected_features(self.lamba))
        return llh1 + llh2 - kl, llh1, llh2, kl
        # FIXME: KL comes out negative...

    def logz(self, lamba):
        lz = 0
        for pid, cid, cnt in edges:
            p1 = self.pcm.prob(pid, cid)
            q1 = p1 / sum(p1) * exp(-lamba)
            q1z = sum(q1)

            p2 = self.cpm.prob(pid, cid)
            q2 = p2 / sum(p2) * exp(-lamba)
            q2z = sum(q2)

            lz += log(0.5 * (q1z + q2z)) * cnt
        return lz

    # z = 1/2 * (sum_y p1(y|x) exp (-lambda . phi(y)) + sum_y p2(y|x) exp (-lambda . phi(y)))
    #   = 1/2 (z1 + z2)
    # d (log z) / dlambda = 1/2 (E_q1 [ -phi ] + E_q2 [ -phi ] )
    def expected_features(self, lamba):
        fs = zeros(num_tags)
        for pid, cid, cnt in edges:
            p1 = self.pcm.prob(pid, cid)
            q1 = (p1 / sum(p1)) * exp(-lamba)
            fs -= 0.5 * cnt * q1 / sum(q1)

            p2 = self.cpm.prob(pid, cid)
            q2 = (p2 / sum(p2)) * exp(-lamba)
            fs -= 0.5 * cnt * q2 / sum(q2)
        return fs

if style == 'p2c':
    m = PhraseToContextModel()
elif style == 'c2p':
    m = ContextToPhraseModel()
elif style == 'prod':
    m = ProductModel()
elif style == 'prodslack':
    m = RegularisedProductModel(0.5)
elif style == 'sum':
    m = InterpolatedModel(0.5)

for iteration in range(30):
    obj = m.expectation_maximisation_step()
    print 'iteration', iteration, 'objective', obj

for pid, cid, cnt in edges:
    p = m.prob(pid, cid)
    phrase = phrase_type_list[pid]
    phrase_str = ' '.join(map(word_type_list.__getitem__, phrase))
    context = context_type_list[cid]
    context_str = ' '.join(map(word_type_list.__getitem__, context))
    print '%s\t%s ||| C=%d' % (phrase_str, context_str, argmax(p))
