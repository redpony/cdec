#!/usr/bin/python

import sys
from operator import itemgetter

if len(sys.argv) <= 2:
  print "Usage: spans2labels.py phrase_context_index [order] [threshold] [languages={s,t,b}{s,t,b}] [type={tag,tok,both},{tag,tok,both}]"
  exit(1)

order=1
threshold = 0
cutoff_cat = "<UNK>"
if len(sys.argv) > 2:
  order = int(sys.argv[2])
if len(sys.argv) > 3:
  threshold = float(sys.argv[3])
phr=ctx='t'
if len(sys.argv) > 4:
  phr, ctx = sys.argv[4]
  assert phr in 'stb'
  assert ctx in 'stb'
phr_typ = ctx_typ = 'both'
if len(sys.argv) > 5:
  phr_typ, ctx_typ = sys.argv[5].split(',')
  assert phr_typ in ('tag', 'tok', 'both')
  assert ctx_typ in ('tag', 'tok', 'both')

#print >>sys.stderr, "Loading phrase index"
phrase_context_index = {}
for line in file(sys.argv[1], 'r'):
  phrase,tail= line.split('\t')
  contexts = tail.split(" ||| ")
  try: # remove Phil's bizarre integer pair
       x,y = contexts[0].split()
       x=int(x); y=int(y)
       contexts = contexts[1:]
  except:
       pass
  if len(contexts) == 1: continue
  assert len(contexts) % 2 == 0
  for i in range(0, len(contexts), 2):
    #parse contexts[i+1] = " C=1 P=0.8 ... "
    features=dict([ keyval.split('=') for keyval in contexts[i+1].split()])
    category = features['C']    
    if features.has_key('P') and float(features['P']) < threshold:
	category = cutoff_cat
    
    phrase_context_index[(phrase,contexts[i])] = category 
    #print (phrase,contexts[i]), category

#print >>sys.stderr, "Labelling spans"
for line in sys.stdin:
  #print >>sys.stderr, "line", line.strip()
  line_segments = line.split(' ||| ')
  assert len(line_segments) >= 3
  source = ['<s>' for x in range(order)] + line_segments[0].split() + ['</s>' for x in range(order)]
  target = ['<s>' for x in range(order)] + line_segments[1].split() + ['</s>' for x in range(order)]
  phrases = [ [int(i) for i in x.split('-')] for x in line_segments[2].split()]

  if phr_typ != 'both' or ctx_typ != 'both':
    if phr in 'tb' or ctx in 'tb':
        target_toks = ['<s>' for x in range(order)] + map(lambda x: x.rsplit('_', 1)[0], line_segments[1].split()) + ['</s>' for x in range(order)]
        target_tags = ['<s>' for x in range(order)] + map(lambda x: x.rsplit('_', 1)[-1], line_segments[1].split()) + ['</s>' for x in range(order)]

        if phr in 'tb':
            if phr_typ == 'tok':
                targetP = target_toks
            elif phr_typ == 'tag':
                targetP = target_tags
        if ctx in 'tb':
            if ctx_typ == 'tok':
                targetC = target_toks
            elif ctx_typ == 'tag':
                targetC = target_tags

    if phr in 'sb' or ctx in 'sb':
        source_toks = ['<s>' for x in range(order)] + map(lambda x: x.rsplit('_', 1)[0], line_segments[0].split()) + ['</s>' for x in range(order)]
        source_tags = ['<s>' for x in range(order)] + map(lambda x: x.rsplit('_', 1)[-1], line_segments[0].split()) + ['</s>' for x in range(order)]

        if phr in 'sb':
            if phr_typ == 'tok':
                sourceP = source_toks
            elif phr_typ == 'tag':
                sourceP = source_tags
        if ctx in 'sb':
            if ctx_typ == 'tok':
                sourceC = source_toks
            elif ctx_typ == 'tag':
                sourceC = source_tags
  else:
    sourceP = sourceC = source
    targetP = targetC = target

  #print >>sys.stderr, "line", source, '---', target, 'phrases', phrases

  print "|||",

  for s1,s2,t1,t2 in phrases:
    s1 += order
    s2 += order
    t1 += order
    t2 += order

    phraset = phrases = contextt = contexts = ''
    if phr in 'tb':
        phraset = reduce(lambda x, y: x+y+" ", targetP[t1:t2], "").strip()
    if phr in 'sb':
        phrases = reduce(lambda x, y: x+y+" ", sourceP[s1:s2], "").strip()

    if ctx in 'tb':
        left_context = reduce(lambda x, y: x+y+" ", targetC[t1-order:t1], "")
        right_context = reduce(lambda x, y: x+y+" ", targetC[t2:t2+order], "").strip()
        contextt = "%s<PHRASE> %s" % (left_context, right_context)
    if ctx in 'sb':
        left_context = reduce(lambda x, y: x+y+" ", sourceC[s1-order:s1], "")
        right_context = reduce(lambda x, y: x+y+" ", sourceC[s2:s2+order], "").strip()
        contexts = "%s<PHRASE> %s" % (left_context, right_context)

    if phr == 'b':
        phrase = phraset + ' <SPLIT> ' + phrases
    elif phr == 's':
        phrase = phrases
    else:
        phrase = phraset

    if ctx == 'b':
        context = contextt + ' <SPLIT> ' + contexts
    elif ctx == 's':
        context = contexts
    else:
        context = contextt

    #print "%d-%d-%d-%d looking up" % (s1-order,s2-order,t1-order,t2-order), (phrase, context)
    label = phrase_context_index.get((phrase,context), cutoff_cat)
    if label != cutoff_cat: #cutoff'd spans are left unlabelled
      print "%d-%d-%d-%d:X%s" % (s1-order,s2-order,t1-order,t2-order,label),
  print
