from __future__ import division
import math

from cdec.sa import isvar

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
        # Always use this for now
        if not ctx.online or ctx.online:
            maxOffScore = 0.0
            for e in ctx.ephrase.words:
                maxScore = max(ttable.get_score(f, e, 0) for f in fwords)
                maxOffScore += -math.log10(maxScore) if maxScore > 0 else MAXSCORE
            return maxOffScore
        else:
            # For now, straight average
            maxOffScore = 0.0
            maxOnScore = 0.0
            for e in ctx.ephrase.words:
                maxScore = max(ttable.get_score(f, e, 0) for f in fwords)
                maxOffScore += -math.log10(maxScore) if maxScore > 0 else MAXSCORE
            for e in ctx.ephrase:
                if not isvar(e):
                    maxScore = 0.0
                    for f in ctx.fphrase:
                        if not isvar(f):
                            b_f = ctx.online.bilex_f.get(f, 0)
                            if b_f:
                                maxScore = max(maxScore, ctx.online.bilex_fe.get(f, {}).get(e))
                    maxOnScore += -math.log10(maxScore) if maxScore > 0 else MAXSCORE
            return (maxOffScore + maxOnScore) / 2
    return MaxLexEgivenF

def MaxLexFgivenE(ttable):
    def MaxLexFgivenE(ctx):
        ewords = ctx.ephrase.words
        ewords.append('NULL')
        # Always use this for now
        if not ctx.online or ctx.online:
            maxOffScore = 0.0
            for f in ctx.fphrase.words:
                maxScore = max(ttable.get_score(f, e, 1) for e in ewords)
                maxOffScore += -math.log10(maxScore) if maxScore > 0 else MAXSCORE
            return maxOffScore
        else:
            # For now, straight average
            maxOffScore = 0.0
            maxOnScore = 0.0
            for f in ctx.fphrase.words:
                maxScore = max(ttable.get_score(f, e, 1) for e in ewords)
                maxOffScore += -math.log10(maxScore) if maxScore > 0 else MAXSCORE
            for f in ctx.fphrase:
                if not isvar(f):
                    maxScore = 0.0
                    for e in ctx.ephrase:
                        if not isvar(e):
                            b_e = ctx.online.bilex_e.get(e, 0)
                            if b_e:
                                maxScore = max(maxScore, ctx.online.bilex_fe.get(f, {}).get(e, 0) / b_e )
                    maxOnScore += -math.log10(maxScore) if maxScore > 0 else MAXSCORE
            return (maxOffScore + maxOnScore) / 2
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
