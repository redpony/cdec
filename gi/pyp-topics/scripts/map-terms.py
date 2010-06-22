#!/usr/bin/python

import sys

if len(sys.argv) != 2:
  print "Usage: map-terms.py vocab-file"
  exit(1)

vocab = file(sys.argv[1], 'r').readlines()
term_dict = map(lambda x: x.strip().replace(' ','_'), vocab)

for line in sys.stdin:
  tokens = line.split()
  for token in tokens:
    elements = token.split(':')
    if len(elements) == 1:
      print "%s" % (term_dict[int(elements[0])]),
    else:
      print "%s:%s" % (term_dict[int(elements[0])], elements[1]),
  print
