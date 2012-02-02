#!/usr/bin/env python
import sys
import model
import sym
import log
import math

class ContextModel(model.Model):
  '''A ContextModel is one that is computed using information
  from the Context object'''

  def __init__(self, context_manager, default=0.0):
    model.Model.__init__(self)
    self.wordless = 0
    self.initial = None
    self.default = default
    self.context_manager = context_manager
    self.id = self.context_manager.add_model(self)

    '''The next feature is true if the model depends in 
    some way on the entire input sentence; that is, if 
    it cannot be scored when created, but must be scored
    no earlier than during the input method (note that
    this is less strict than stateful)'''
    self.contextual = True 
    ''' It may seem somewhat counterintuitive that a
    ContextModel can be non-contextual, but a good
    example is the rule probabilites; although these
    are computed using the Context object, they don't
    really depend in any way on context'''


  '''inherited from model.Model, called once for each input sentence'''
  def input(self, fwords, meta):
    # all ContextModels must make this call
    self.context_manager.input(self, fwords, meta)


  '''This function will be called via the input method
  only for contextual models'''
  def compute_contextual_score(self, r):
    return 0.0

  '''This function is only called on rule creation for
  contextless models'''
  def compute_contextless_score(self, fphrase, ephrase, paircount, fcount, fsample_count):
    return 0.0

  '''Stateless models should not need to 
  override this function, unless they define
  something for model.TO_GOAL'''
  def transition (self,  r, antstates, i, j, j1=None):
    return (None, 0.0)

  def estimate(self, r):
    return r.getscore("context", self.id)

  def transition(self, r, antstates, i, j, j1=None):
    return (None, r.getscore("context", self.id))

  def finaltransition(self, state):
    return 0.0

  def rescore(self, ewords, score):
    return score



'''p(e|f)'''
class EgivenF(ContextModel):
  
  def __init__(self, context_manager, default=0.0):
    ContextModel.__init__(self, context_manager)
    self.contextual = False

  def compute_contextless_score(self, fphrase, ephrase, paircount, fcount, fsample_count):
    prob = float(paircount)/float(fcount)
    return -math.log10(prob)

class CountEF(ContextModel):

        def __init__(self, context_manager, default=0.0):
                ContextModel.__init__(self, context_manager)
                self.contextual = False

        def compute_contextless_score(self, fphrase, ephrase, paircount, fcount, fsample_count):
                return math.log10(1.0 + float(paircount))

class SampleCountF(ContextModel):

        def __init__(self, context_manager, default=0.0):
                ContextModel.__init__(self, context_manager)
                self.contextual = False

        def compute_contextless_score(self, fphrase, ephrase, paircount, fcount, fsample_count):
                return math.log10(1.0 + float(fsample_count))



class EgivenFCoherent(ContextModel):

  def __init__(self, context_manager, default=0.0):
    ContextModel.__init__(self, context_manager)
    self.contextual = False

  def compute_contextless_score(self, fphrase, ephrase, paircount, fcount, fsample_count):
    prob = float(paircount)/float(fsample_count)
    #print "paircount=",paircount," , fsample_count=",fsample_count,", prob=",prob
    if (prob == 0.0): return 99.0
    return -math.log10(prob)



class CoherenceProb(ContextModel):

  def __init__(self, context_manager, default=0.0):
    ContextModel.__init__(self, context_manager)
    self.contextual = False

  def compute_contextless_score(self, fphrase, ephrase, paircount, fcount, fsample_count):
    prob = float(fcount)/float(fsample_count)
    return -math.log10(prob)



class MaxLexEgivenF(ContextModel):

  def __init__(self, context_manager, ttable, col=0):
    ContextModel.__init__(self, context_manager)
    self.ttable = ttable
    self.col = col
    self.wordless = 0
    self.initial = None
    self.contextual = False


  def compute_contextless_score(self, fphrase, ephrase, paircount, fcount, fsample_count):
    totalscore = 1.0
    fwords = map(sym.tostring, filter(lambda x: not sym.isvar(x), fphrase))
    fwords.append("NULL")
    ewords = map(sym.tostring, filter(lambda x: not sym.isvar(x), ephrase))
    for e in ewords:
      maxScore = 0.0
      for f in fwords:
        score = self.ttable.get_score(f, e, self.col)
        #print "score(MaxLexEgivenF) = ",score
        if score > maxScore:
          maxScore = score
      totalscore *= maxScore
    if totalscore == 0.0:
      return 999
    else:
      return -math.log10(totalscore)


class MaxLexFgivenE(ContextModel):

  def __init__(self, context_manager, ttable, col=1):
    ContextModel.__init__(self, context_manager)
    self.ttable = ttable
    self.col = col
    self.wordless = 0
    self.initial = None
    self.contextual = False


  def compute_contextless_score(self, fphrase, ephrase, paircount, fcount, fsample_count):
    totalscore = 1.0
    fwords = map(sym.tostring, filter(lambda x: not sym.isvar(x), fphrase))
    ewords = map(sym.tostring, filter(lambda x: not sym.isvar(x), ephrase))
    ewords.append("NULL")
    for f in fwords:
      maxScore = 0.0
      for e in ewords:
        score = self.ttable.get_score(f, e, self.col)
        #print "score(MaxLexFgivenE) = ",score
        if score > maxScore:
          maxScore = score
      totalscore *= maxScore
    if totalscore == 0.0:
      return 999
    else:
      return -math.log10(totalscore)


class IsSingletonF(ContextModel):

        def __init__(self, context_manager, default=0.0):
                ContextModel.__init__(self, context_manager)
                self.contextual = False

        def compute_contextless_score(self, fphrase, ephrase, paircount, fcount, fsample_count):
                return (fcount==1)


class IsSingletonFE(ContextModel):

        def __init__(self, context_manager, default=0.0):
                ContextModel.__init__(self, context_manager)
                self.contextual = False

        def compute_contextless_score(self, fphrase, ephrase, paircount, fcount, fsample_count):
                return (paircount==1)

class IsNotSingletonF(ContextModel):

        def __init__(self, context_manager, default=0.0):
                ContextModel.__init__(self, context_manager)
                self.contextual = False

        def compute_contextless_score(self, fphrase, ephrase, paircount, fcount, fsample_count):
                return (fcount>1)


class IsNotSingletonFE(ContextModel):

        def __init__(self, context_manager, default=0.0):
                ContextModel.__init__(self, context_manager)
                self.contextual = False

        def compute_contextless_score(self, fphrase, ephrase, paircount, fcount, fsample_count):
                return (paircount>1)


class IsFEGreaterThanZero(ContextModel):

        def __init__(self, context_manager, default=0.0):
                ContextModel.__init__(self, context_manager)
                self.contextual = False

        def compute_contextless_score(self, fphrase, ephrase, paircount, fcount, fsample_count):
                return (paircount > 0.01)


