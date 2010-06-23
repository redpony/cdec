#!/usr/bin/python

import sys
from operator import itemgetter

if len(sys.argv) > 3:
  print "Usage: contexts2documents.py [contexts_index_out] [phrases_index_out]"
  exit(1)

context_index = {} 
phrase_index = {}
for line in sys.stdin:
  phrase, line_tail = line.split('\t')

  raw_contexts = line_tail.split('|||')
  contexts = [c.strip() for x,c in enumerate(raw_contexts) if x%2 == 0]
  counts   = [int(c.split('=')[1].strip()) for x,c in enumerate(raw_contexts) if x%2 != 0]
  phrase_index.setdefault(phrase, len(phrase_index))
  print len(contexts),
  for context,count in zip(contexts,counts): 
    c = context_index.setdefault(context, len(context_index))
    print "%d:%d" % (c,count),
  print
if 1 < len(sys.argv) < 4:
  contexts_out = open(sys.argv[1],'w')
  contexts = context_index.items()
  contexts.sort(key = itemgetter(1))
  for context in contexts: 
    print >>contexts_out, context[0]
  contexts_out.close()
if len(sys.argv) == 3:
  phrases_out = open(sys.argv[2],'w')
  phrases = phrase_index.items()
  phrases.sort(key = itemgetter(1))
  for phrase in phrases: 
    print >>phrases_out, phrase[0]
  phrases_out.close()
