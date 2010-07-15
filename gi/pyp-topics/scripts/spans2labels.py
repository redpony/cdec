#!/usr/bin/python

import sys
from operator import itemgetter

if len(sys.argv) <= 2:
  print "Usage: spans2labels.py phrase_context_index [order] [threshold] [languages={s,t,b}{s,t,b}]"
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
#   print (phrase,contexts[i]), category, prob

for line in sys.stdin:
  line_segments = line.split('|||')
  source = ['<s>' for x in range(order)] + line_segments[0].split() + ['</s>' for x in range(order)]
  target = ['<s>' for x in range(order)] + line_segments[1].split() + ['</s>' for x in range(order)]
  phrases = [ [int(i) for i in x.split('-')] for x in line_segments[2].split()]

  print "|||",

  for s1,s2,t1,t2 in phrases:
    s1 += order
    s2 += order
    t1 += order
    t2 += order

    phraset = phrases = contextt = contexts = ''
    if phr in 'tb':
        phraset = reduce(lambda x, y: x+y+" ", target[t1:t2], "").strip()
    if phr in 'sb':
        phrases = reduce(lambda x, y: x+y+" ", source[s1:s2], "").strip()

    if ctx in 'tb':
        left_context = reduce(lambda x, y: x+y+" ", target[t1-order:t1], "")
        right_context = reduce(lambda x, y: x+y+" ", target[t2:t2+order], "").strip()
        contextt = "%s<PHRASE> %s" % (left_context, right_context)
    if ctx in 'sb':
        left_context = reduce(lambda x, y: x+y+" ", source[s1-order:s1], "")
        right_context = reduce(lambda x, y: x+y+" ", source[s2:s2+order], "").strip()
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

    label = phrase_context_index.get((phrase,context), "<UNK>")
    if label != cutoff_cat: #cutoff'd spans are left unlabelled
      print "%d-%d-%d-%d:X%s" % (s1-order,s2-order,t1-order,t2-order,label),
  print


