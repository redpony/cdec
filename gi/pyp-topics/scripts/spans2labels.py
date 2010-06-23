#!/usr/bin/python

import sys
from operator import itemgetter

if len(sys.argv) != 4:
  print "Usage: spans2labels.py phrase_index context_index phrase_context_index"
  exit(1)

phrase_index = dict(map(lambda x: (x[1].strip(),x[0]), enumerate(file(sys.argv[1], 'r').readlines())))
context_index = dict(map(lambda x: (x[1].strip(),x[0]), enumerate(file(sys.argv[2], 'r').readlines())))

phrase_context_index = {}
for i,line in enumerate(file(sys.argv[3], 'r').readlines()):
  for c,l in map(lambda x: x.split(':'), line.split()[1:]):
    phrase_context_index[(int(i),int(c))] = l

for line in sys.stdin:
  line_segments = line.split('|||')
  source = ['<s>'] + line_segments[0].split() + ['</s>']
  target = ['<s>'] + line_segments[1].split() + ['</s>']
  phrases = [ [int(i) for i in x.split('-')] for x in line_segments[2].split()]

# for x in source[1:-1]: 
#   print x,
# print "|||",
# for x in target[1:-1]: 
#   print x,
  print "|||",

  for s1,s2,t1,t2 in phrases:
    s1 += 1
    s2 += 1
    t1 += 1
    t2 += 1

    phrase = reduce(lambda x, y: x+y+" ", target[t1:t2], "").strip()
    context = "%s <PHRASE> %s" % (target[t1-1], target[t2])

    pi = phrase_index[phrase]
    ci = context_index[context]
    label = phrase_context_index[(pi,ci)]
    print "%s-%s:X%s" % (t1-1,t2-1,label),
#   print phrase, pi, context, ci
#   print phrase_context_index[(pi,ci)]
  print
