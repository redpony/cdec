#!/usr/bin/python

import sys

if len(sys.argv) != 2:
  print "Usage: topics.py words-per-topic"
  exit(1)

for t,line in enumerate(sys.stdin):
  tokens = line.split()
  terms = []
  for token in tokens:
    elements = token.rsplit(':',1)
    terms.append((int(elements[1]),elements[0]))
  terms.sort()
  terms.reverse()

  print "Topic %d:" % t
  map(lambda (x,y) : sys.stdout.write("   %s:%s\n" % (y,x)), terms[:int(sys.argv[1])])
  print
