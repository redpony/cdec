import sys
import scipy.optimize
from numpy import *
from numpy.random import random

#
# Step 1: load the concordance counts
# 

edges_phrase_to_context = {}
edges_context_to_phrase = {}
types = {}
num_edges = 0

for line in sys.stdin:
    phrase, rest = line.strip().split('\t')
    parts = rest.split('|||')
    for i in range(0, len(parts), 2):
        context, count = parts[i:i+2]

        ctx = tuple(filter(lambda x: x != '<PHRASE>', context.split()))
        cnt = int(count.strip()[2:])
        ccs = edges_phrase_to_context.setdefault(phrase, {})
        ccs[ctx] = cnt
        pcs = edges_context_to_phrase.setdefault(ctx, {})
        pcs[phrase] = cnt

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
delta = int(sys.argv[1])
gamma = int(sys.argv[2])

def normalise(a):
    return a / sum(a)

# Pr(tag)
tagDist = normalise(random(num_tags)+1)
# Pr(context at pos i = w | tag) indexed by i, tag, word
contextWordDist = [[normalise(random(num_types)+1) for t in range(num_tags)] for i in range(4)]
# PR langrange multipliers
lamba = zeros(2 * num_edges * num_tags)
omega_offset = num_edges * num_tags
lamba_index = {}
next = 0
for phrase, ccs in edges_phrase_to_context.items():
    for context in ccs.keys():
        lamba_index[phrase,context] = next
        next += num_tags

#
# Step 3: expectation maximisation
#

for iteration in range(20):
    tagCounts = zeros(num_tags)
    contextWordCounts = [[zeros(num_types) for t in range(num_tags)] for i in range(4)]

    #print 'tagDist', tagDist
    #print 'contextWordCounts[0][0]', contextWordCounts[0][0]

    # Tune lambda
    # dual: min log Z(lamba) s.t. lamba >= 0;
    # sum_c lamba_pct <= delta; sum_p lamba_pct <= gamma
    def dual(ls):
        logz = 0
        for phrase, ccs in edges_phrase_to_context.items():
            for context, count in ccs.items():
                conditionals = zeros(num_tags)
                for t in range(num_tags):
                    prob = tagDist[t]
                    for i in range(4):
                        prob *= contextWordDist[i][t][types[context[i]]]
                    conditionals[t] = prob
                cz = sum(conditionals)
                conditionals /= cz

                local_z = 0
                for t in range(num_tags):
                    li = lamba_index[phrase,context] + t
                    local_z += conditionals[t] * exp(-ls[li] - ls[omega_offset+li])
                logz += log(local_z) * count

        #print 'ls', ls
        #print 'lambda', list(ls)
        #print 'dual', logz
        return logz

    def loglikelihood():
        llh = 0
        for phrase, ccs in edges_phrase_to_context.items():
            for context, count in ccs.items():
                conditionals = zeros(num_tags)
                for t in range(num_tags):
                    prob = tagDist[t]
                    for i in range(4):
                        prob *= contextWordDist[i][t][types[context[i]]]
                    conditionals[t] = prob
                cz = sum(conditionals)
                llh += log(cz) * count
        return llh

    def primal(ls):
        # FIXME: returns negative values for KL (impossible)
        logz = dual(ls)
        expectations = -dual_deriv(ls)
        kl = -logz - dot(ls, expectations)
        llh = loglikelihood()

        pt_l1linf = 0
        for phrase, ccs in edges_phrase_to_context.items():
            for t in range(num_tags):
                best = -1e500
                for context, count in ccs.items():
                    li = lamba_index[phrase,context] + t
                    s = expectations[li]
                    if s > best: best = s
                pt_l1linf += best

        ct_l1linf = 0
        for context, pcs in edges_context_to_phrase.items():
            for t in range(num_tags):
                best = -1e500
                for phrase, count in pcs.items():
                    li = lamba_index[phrase,context] + t
                    s = expectations[li]
                    if s > best: best = s
                ct_l1linf += best

        return llh, kl, pt_l1linf, ct_l1linf, llh + kl + delta * pt_l1linf + gamma * ct_l1linf

    def dual_deriv(ls):
        # d/dl log(z) = E_q[phi]
        deriv = zeros(2 * num_edges * num_tags)
        for phrase, ccs in edges_phrase_to_context.items():
            for context, count in ccs.items():
                conditionals = zeros(num_tags)
                for t in range(num_tags):
                    prob = tagDist[t]
                    for i in range(4):
                        prob *= contextWordDist[i][t][types[context[i]]]
                    conditionals[t] = prob
                cz = sum(conditionals)
                conditionals /= cz

                scores = zeros(num_tags)
                for t in range(num_tags):
                    li = lamba_index[phrase,context] + t
                    scores[t] = conditionals[t] * exp(-ls[li] - ls[omega_offset + li])
                local_z = sum(scores)

                for t in range(num_tags):
                    if delta > 0:
                        deriv[lamba_index[phrase,context] + t] -= count * scores[t] / local_z
                    if gamma > 0:
                        deriv[omega_offset + lamba_index[phrase,context] + t] -= count * scores[t] / local_z

        #print 'ddual', deriv
        return deriv

    def constraints(ls):
        cons = zeros(num_phrases * num_tags + num_edges * num_tags)

        index = 0
        for phrase, ccs in edges_phrase_to_context.items():
            for t in range(num_tags):
                if delta > 0:
                    total = delta
                    for cprime in ccs.keys():
                        total -= ls[lamba_index[phrase, cprime] + t]
                    cons[index] = total
                index += 1

        for context, pcs in edges_context_to_phrase.items():
            for t in range(num_tags):
                if gamma > 0:
                    total = gamma
                    for pprime in pcs.keys():
                        total -= ls[omega_offset + lamba_index[pprime, context] + t]
                    cons[index] = total
                index += 1

        #print 'cons', cons
        return cons

    def constraints_deriv(ls):
        cons = zeros((num_phrases * num_tags + num_edges * num_tags, 2 * num_edges * num_tags))

        index = 0
        for phrase, ccs in edges_phrase_to_context.items():
            for t in range(num_tags):
                if delta > 0:
                    d = cons[index,:]#zeros(num_edges * num_tags)
                    for cprime in ccs.keys():
                        d[lamba_index[phrase, cprime] + t] = -1
                    #cons[index] = d
                index += 1

        for context, pcs in edges_context_to_phrase.items():
            for t in range(num_tags):
                if gamma > 0:
                    d = cons[index,:]#d = zeros(num_edges * num_tags)
                    for pprime in pcs.keys():
                        d[omega_offset + lamba_index[pprime, context] + t] = -1
                    #cons[index] = d
                index += 1
        #print 'dcons', cons
        return cons

    print 'Pre lambda optimisation dual', dual(lamba), 'primal', primal(lamba)
    #print 'lambda', lamba, lamba.shape
    #print 'bounds', [(0, max(delta, gamma))] * (2 * num_edges * num_tags)

    lamba = scipy.optimize.fmin_slsqp(dual, lamba,
                            bounds=[(0, max(delta, gamma))] * (2 * num_edges * num_tags),
                            f_ieqcons=constraints,
                            fprime=dual_deriv,
                            fprime_ieqcons=constraints_deriv,
                            iprint=0)
    print 'Post lambda optimisation dual', dual(lamba), 'primal', primal(lamba)

    # E-step
    llh = z = 0
    for phrase, ccs in edges_phrase_to_context.items():
        for context, count in ccs.items():
            conditionals = zeros(num_tags)
            for t in range(num_tags):
                prob = tagDist[t]
                for i in range(4):
                    prob *= contextWordDist[i][t][types[context[i]]]
                conditionals[t] = prob
            cz = sum(conditionals)
            conditionals /= cz
            llh += log(cz) * count

            scores = zeros(num_tags)
            li = lamba_index[phrase, context]
            for t in range(num_tags):
                scores[t] = conditionals[t] * exp(-lamba[li + t] - lamba[omega_offset + li + t])
            z += count * sum(scores)

            tagCounts += count * scores
            for i in range(4):
                for t in range(num_tags):
                    contextWordCounts[i][t][types[context[i]]] += count * scores[t]

    print 'iteration', iteration, 'llh', llh, 'logz', log(z)

    # M-step
    tagDist = normalise(tagCounts)
    for i in range(4):
        for t in range(num_tags):
            contextWordDist[i][t] = normalise(contextWordCounts[i][t])


for phrase, ccs in edges_phrase_to_context.items():
    for context, count in ccs.items():
        conditionals = zeros(num_tags)
        for t in range(num_tags):
            prob = tagDist[t]
            for i in range(4):
                prob *= contextWordDist[i][t][types[context[i]]]
            conditionals[t] = prob
        cz = sum(conditionals)
        conditionals /= cz

        #scores = zeros(num_tags)
        #li = lamba_index[phrase, context]
        #for t in range(num_tags):
        #    scores[t] = conditionals[t] * exp(-lamba[li + t])

        #print '%s\t%s ||| C=%d ||| %d |||' % (phrase, context, count, argmax(scores)), scores / sum(scores)
        print '%s\t%s ||| C=%d ||| %d |||' % (phrase, context, count, argmax(conditionals)), conditionals
