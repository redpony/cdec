import sys
import scipy.optimize
from numpy import *
from numpy.random import random, seed

#
# Step 1: load the concordance counts
# 

edges_phrase_to_context = []
edges_context_to_phrase = []
types = {}
context_types = {}
num_edges = 0

for line in sys.stdin:
    phrase, rest = line.strip().split('\t')
    parts = rest.split('|||')
    edges_phrase_to_context.append((phrase, []))
    for i in range(0, len(parts), 2):
        context, count = parts[i:i+2]

        ctx = tuple(filter(lambda x: x != '<PHRASE>', context.split()))
        cnt = int(count.strip()[2:])
        edges_phrase_to_context[-1][1].append((ctx, cnt))

        cid = context_types.get(ctx, len(context_types))
        if cid == len(context_types):
            context_types[ctx] = cid
            edges_context_to_phrase.append((ctx, []))
        edges_context_to_phrase[cid][1].append((phrase, cnt))

        for token in ctx:
            types.setdefault(token, len(types))
        for token in phrase.split():
            types.setdefault(token, len(types))

        num_edges += 1

print 'Read in', num_edges, 'edges and', len(types), 'word types'

#
# Step 2: initialise the model parameters
#

num_tags = 5
num_types = len(types)
num_phrases = len(edges_phrase_to_context)
num_contexts = len(edges_context_to_phrase)
delta = float(sys.argv[1])
assert sys.argv[2] in ('local', 'global')
local = sys.argv[2] == 'local'
if len(sys.argv) >= 2:
     seed(int(sys.argv[3]))

def normalise(a):
    return a / float(sum(a))

# Pr(tag | phrase)
tagDist = [normalise(random(num_tags)+1) for p in range(num_phrases)]
# Pr(context at pos i = w | tag) indexed by i, tag, word
contextWordDist = [[normalise(random(num_types)+1) for t in range(num_tags)] for i in range(4)]

#
# Step 3: expectation maximisation
#

class GlobalDualObjective:
    """
    Objective, log(z), for all phrases s.t. lambda >= 0, sum_c lambda_pct <= scale 
    """

    def __init__(self, scale):
        self.scale = scale
        self.posterior = zeros((num_edges, num_tags))
        self.q = zeros((num_edges, num_tags))
        self.llh = 0

        index = 0
        for j, (phrase, edges) in enumerate(edges_phrase_to_context):
            for context, count in edges:
                for t in range(num_tags):
                    prob = tagDist[j][t]
                    for k, token in enumerate(context):
                        prob *= contextWordDist[k][t][types[token]]
                    self.posterior[index,t] = prob
                z = sum(self.posterior[index,:])
                self.posterior[index,:] /= z
                self.llh += log(z)
                index += 1

    def objective(self, ls):
        ls = ls.reshape((num_edges, num_tags))
        logz = 0

        index = 0
        for j, (phrase, edges) in enumerate(edges_phrase_to_context):
            for context, count in edges:
                for t in range(num_tags):
                    self.q[index,t] = self.posterior[index,t] * exp(-ls[index,t])
                local_z = sum(self.q[index,:])
                self.q[index,:] /= local_z
                logz += log(local_z) * count
                index += 1

        return logz

    # FIXME: recomputes q many more times than necessary

    def gradient(self, ls):
        ls = ls.reshape((num_edges, num_tags))
        gradient = zeros((num_edges, num_tags))

        index = 0
        for j, (phrase, edges) in enumerate(edges_phrase_to_context):
            for context, count in edges:
                for t in range(num_tags):
                    self.q[index,t] = self.posterior[index,t] * exp(-ls[index,t])
                local_z = sum(self.q[index,:])
                self.q[index,:] /= local_z
                for t in range(num_tags):
                    gradient[index,t] -= self.q[index,t] * count
                index += 1

        return gradient.ravel()

    def constraints(self, ls):
        ls = ls.reshape((num_edges, num_tags))
        cons = ones((num_phrases, num_tags)) * self.scale
        index = 0
        for j, (phrase, edges) in enumerate(edges_phrase_to_context):
            for i, (context, count) in enumerate(edges):
                for t in range(num_tags):
                    cons[j,t] -= ls[index,t]
                index += 1
        return cons.ravel()

    def constraints_gradient(self, ls):
        ls = ls.reshape((num_edges, num_tags))
        gradient = zeros((num_phrases, num_tags, num_edges, num_tags))
        index = 0
        for j, (phrase, edges) in enumerate(edges_phrase_to_context):
            for i, (context, count) in enumerate(edges):
                for t in range(num_tags):
                    gradient[j,t,index,t] -= 1
                index += 1
        return gradient.reshape((num_phrases*num_tags, num_edges*num_tags))

    def optimize(self):
        ls = zeros(num_edges * num_tags)
        #print '\tpre lambda optimisation dual', self.objective(ls) #, 'primal', primal(lamba)
        ls = scipy.optimize.fmin_slsqp(self.objective, ls,
                                bounds=[(0, self.scale)] * num_edges * num_tags,
                                f_ieqcons=self.constraints,
                                fprime=self.gradient,
                                fprime_ieqcons=self.constraints_gradient,
                                iprint=0) # =2 for verbose
        #print '\tpost lambda optimisation dual', self.objective(ls) #, 'primal', primal(lamba)

        # returns llh, kl and l1lmax contribution
        l1lmax = 0
        index = 0
        for j, (phrase, edges) in enumerate(edges_phrase_to_context):
            for t in range(num_tags):
                lmax = None
                for i, (context, count) in enumerate(edges):
                    lmax = max(lmax, self.q[index+i,t])
                l1lmax += lmax
            index += len(edges)

        return self.llh, -self.objective(ls) + dot(ls, self.gradient(ls)), l1lmax

class LocalDualObjective:
    """
    Local part of objective, log(z) relevant to lambda_p**.
    Optimised subject to lambda >= 0, sum_c lambda_pct <= scale forall t 
    """

    def __init__(self, phraseId, scale):
        self.phraseId = phraseId
        self.scale = scale
        edges = edges_phrase_to_context[self.phraseId][1]
        self.posterior = zeros((len(edges), num_tags))
        self.q = zeros((len(edges), num_tags))
        self.llh = 0

        for i, (context, count) in enumerate(edges):
            for t in range(num_tags):
                prob = tagDist[phraseId][t]
                for j, token in enumerate(context):
                    prob *= contextWordDist[j][t][types[token]]
                self.posterior[i,t] = prob
            z = sum(self.posterior[i,:])
            self.posterior[i,:] /= z
            self.llh += log(z)

    def objective(self, ls):
        edges = edges_phrase_to_context[self.phraseId][1]
        ls = ls.reshape((len(edges), num_tags))
        logz = 0

        for i, (context, count) in enumerate(edges):
            for t in range(num_tags):
                self.q[i,t] = self.posterior[i,t] * exp(-ls[i,t])
            local_z = sum(self.q[i,:])
            self.q[i,:] /= local_z
            logz += log(local_z) * count

        return logz

    # FIXME: recomputes q many more times than necessary

    def gradient(self, ls):
        edges = edges_phrase_to_context[self.phraseId][1]
        ls = ls.reshape((len(edges), num_tags))
        gradient = zeros((len(edges), num_tags))

        for i, (context, count) in enumerate(edges):
            for t in range(num_tags):
                self.q[i,t] = self.posterior[i,t] * exp(-ls[i,t])
            local_z = sum(self.q[i,:])
            self.q[i,:] /= local_z
            for t in range(num_tags):
                gradient[i,t] -= self.q[i,t] * count

        return gradient.ravel()

    def constraints(self, ls):
        edges = edges_phrase_to_context[self.phraseId][1]
        ls = ls.reshape((len(edges), num_tags))
        cons = ones(num_tags) * self.scale
        for t in range(num_tags):
            for i, (context, count) in enumerate(edges):
                cons[t] -= ls[i,t]
        return cons

    def constraints_gradient(self, ls):
        edges = edges_phrase_to_context[self.phraseId][1]
        ls = ls.reshape((len(edges), num_tags))
        gradient = zeros((num_tags, len(edges), num_tags))
        for t in range(num_tags):
            for i, (context, count) in enumerate(edges):
                gradient[t,i,t] -= 1
        return gradient.reshape((num_tags, len(edges)*num_tags))

    def optimize(self):
        edges = edges_phrase_to_context[self.phraseId][1]
        ls = zeros(len(edges) * num_tags)
        #print '\tpre lambda optimisation dual', self.objective(ls) #, 'primal', primal(lamba)
        ls = scipy.optimize.fmin_slsqp(self.objective, ls,
                                bounds=[(0, self.scale)] * len(edges) * num_tags,
                                f_ieqcons=self.constraints,
                                fprime=self.gradient,
                                fprime_ieqcons=self.constraints_gradient,
                                iprint=0) # =2 for verbose
        #print '\tpost lambda optimisation dual', self.objective(ls) #, 'primal', primal(lamba)

        # returns llh, kl and l1lmax contribution
        l1lmax = 0
        for t in range(num_tags):
            lmax = None
            for i, (context, count) in enumerate(edges):
                lmax = max(lmax, self.q[i,t])
            l1lmax += lmax

        return self.llh, -self.objective(ls) + dot(ls, self.gradient(ls)), l1lmax

for iteration in range(20):
    tagCounts = [zeros(num_tags) for p in range(num_phrases)]
    contextWordCounts = [[zeros(num_types) for t in range(num_tags)] for i in range(4)]

    # E-step
    llh = kl = l1lmax = 0
    if local:
        for p in range(num_phrases):
            o = LocalDualObjective(p, delta)
            #print '\toptimising lambda for phrase', p, '=', edges_phrase_to_context[p][0]
            obj = o.optimize()
            print '\tphrase', p, 'deltas', obj
            llh += obj[0]
            kl += obj[1]
            l1lmax += obj[2]

            edges = edges_phrase_to_context[p][1]
            for j, (context, count) in enumerate(edges):
                for t in range(num_tags):
                    tagCounts[p][t] += count * o.q[j,t]
                for i in range(4):
                    for t in range(num_tags):
                        contextWordCounts[i][t][types[context[i]]] += count * o.q[j,t]

        #print 'iteration', iteration, 'LOCAL objective', (llh + kl + delta * l1lmax), 'llh', llh, 'kl', kl, 'l1lmax', l1lmax
    else:
        o = GlobalDualObjective(delta)
        obj = o.optimize()
        llh, kl, l1lmax = o.optimize()

        index = 0
        for p, (phrase, edges) in enumerate(edges_phrase_to_context):
            for context, count in edges:
                for t in range(num_tags):
                    tagCounts[p][t] += count * o.q[index,t]
                for i in range(4):
                    for t in range(num_tags):
                        contextWordCounts[i][t][types[context[i]]] += count * o.q[index,t]
                index += 1

    print 'iteration', iteration, 'objective', (llh + kl + delta * l1lmax), 'llh', llh, 'kl', kl, 'l1lmax', l1lmax

    # M-step
    for p in range(num_phrases):
        tagDist[p] = normalise(tagCounts[p])
    for i in range(4):
        for t in range(num_tags):
            contextWordDist[i][t] = normalise(contextWordCounts[i][t])

for p, (phrase, ccs) in enumerate(edges_phrase_to_context):
    for context, count in ccs:
        conditionals = zeros(num_tags)
        for t in range(num_tags):
            prob = tagDist[p][t]
            for i in range(4):
                prob *= contextWordDist[i][t][types[context[i]]]
            conditionals[t] = prob
        cz = sum(conditionals)
        conditionals /= cz

        print '%s\t%s ||| C=%d ||| %d |||' % (phrase, context, count, argmax(conditionals)), conditionals
