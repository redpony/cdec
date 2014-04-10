from __future__ import division
import math

from cdec.sa import isvar

from online import get_score_multilex

MAXSCORE = 99

def EgivenF(ctx): # p(e|f) = c(e, f)/c(f)
    if not ctx.online:
        prob = ctx.paircount/ctx.fcount
    else:
        prob = (ctx.paircount + ctx.online.paircount) / (ctx.fcount + ctx.online.fcount)
    return -math.log10(prob)

def CountEF(ctx): # c(e, f)
    if not ctx.online:
        count = 1 + ctx.paircount
    else:
        count = 1 + ctx.paircount + ctx.online.paircount
    return math.log10(count)

def SampleCountF(ctx): # sample c(f)
    if not ctx.online:
        count = 1 + ctx.fsample_count
    else:
        count = 1 + ctx.fsample_count + ctx.online.fsample_count
    return math.log10(count)

def EgivenFCoherent(ctx): # c(e, f) / sample c(f)
    if not ctx.online:
        prob = ctx.paircount/ctx.fsample_count
    else:
        prob = (ctx.paircount + ctx.online.paircount) / (ctx.fsample_count + ctx.online.fsample_count)
    return -math.log10(prob) if prob > 0 else MAXSCORE

def CoherenceProb(ctx): # c(f) / sample c(f)
    if not ctx.online:
        prob = ctx.fcount/ctx.fsample_count
    else:
        prob = (ctx.fcount + ctx.online.fcount) / (ctx.fsample_count + ctx.online.fsample_count)
    return -math.log10(prob)

def MaxLexEgivenF(ttable):
    def MaxLexEgivenF(ctx):
        fwords = ctx.fphrase.words
        fwords.append('NULL')
        maxOffScore = 0.0
        for e in ctx.ephrase.words:
            if ctx.online:
                maxScore = max(get_score_multilex(f, e, 0, (ttable, ctx.online.bilex)) for f in fwords)
            else:
                maxScore = max(ttable.get_score(f, e, 0) for f in fwords)
            maxOffScore += -math.log10(maxScore) if maxScore > 0 else MAXSCORE
        return maxOffScore
    return MaxLexEgivenF

def MaxLexFgivenE(ttable):
    def MaxLexFgivenE(ctx):
        ewords = ctx.ephrase.words
        ewords.append('NULL')
        maxOffScore = 0.0
        for f in ctx.fphrase.words:
            if ctx.online:
                maxScore = max(get_score_multilex(f, e, 1, (ttable, ctx.online.bilex)) for e in ewords)
            else:
                maxScore = max(ttable.get_score(f, e, 1) for e in ewords)
            maxOffScore += -math.log10(maxScore) if maxScore > 0 else MAXSCORE
        return maxOffScore
    return MaxLexFgivenE

def IsSingletonF(ctx):
    if not ctx.online:
        count = ctx.fcount
    else:
        count = ctx.fcount + ctx.online.fcount  
    return math.fabs(count - 1) < 1e-6

def IsSingletonFE(ctx):
    if not ctx.online:
        count = ctx.paircount
    else:
        count = ctx.paircount + ctx.online.paircount
    return (count == 1)

def IsNotSingletonF(ctx):
    if not ctx.online:
        count = ctx.fcount
    else:
        count = ctx.fcount + ctx.online.fcount
    return (count > 1)

def IsNotSingletonFE(ctx):
    if not ctx.online:
        count = ctx.paircount
    else:
        count = ctx.paircount + ctx.online.paircount
    return (ctx.paircount > 1)

def IsFEGreaterThanZero(ctx):
    if not ctx.online:
        count = ctx.paircount
    else:
        count = ctx.paircount + ctx.online.paircount
    return (ctx.paircount > 0.01)

def IsSupportedOnline(ctx): # Occurs in online data?
    if ctx.online:
        return (ctx.online.paircount > 0.01)
    else:
        return False
