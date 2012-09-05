from __future__ import division
import math

MAXSCORE = 99

def EgivenF(ctx): # p(e|f) = c(e, f)/c(f)
    return -math.log10(ctx.paircount/ctx.fcount)

def CountEF(ctx): # c(e, f)
    return math.log10(1 + ctx.paircount)

def SampleCountF(ctx): # sample c(f)
    return math.log10(1 + ctx.fsample_count)

def EgivenFCoherent(ctx): # c(e, f) / sample c(f)
    prob = ctx.paircount/ctx.fsample_count
    return -math.log10(prob) if prob > 0 else MAXSCORE

def CoherenceProb(ctx): # c(f) / sample c(f)
    return -math.log10(ctx.fcount/ctx.fsample_count)

def MaxLexEgivenF(ttable):
    def MaxLexEgivenF(ctx):
        fwords = ctx.fphrase.words
        fwords.append('NULL')
        def score():
            for e in ctx.ephrase.words:
              maxScore = max(ttable.get_score(f, e, 0) for f in fwords)
              yield -math.log10(maxScore) if maxScore > 0 else MAXSCORE
        return sum(score())
    return MaxLexEgivenF

def MaxLexFgivenE(ttable):
    def MaxLexFgivenE(ctx):
        ewords = ctx.ephrase.words
        ewords.append('NULL')
        def score():
            for f in ctx.fphrase.words:
              maxScore = max(ttable.get_score(f, e, 1) for e in ewords)
              yield -math.log10(maxScore) if maxScore > 0 else MAXSCORE
        return sum(score())
    return MaxLexFgivenE

def IsSingletonF(ctx):
    return (ctx.fcount == 1)

def IsSingletonFE(ctx):
    return (ctx.paircount == 1)

def IsNotSingletonF(ctx):
    return (ctx.fcount > 1)

def IsNotSingletonFE(ctx):
    return (ctx.paircount > 1)

def IsFEGreaterThanZero(ctx):
    return (ctx.paircount > 0.01)
