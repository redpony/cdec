#!/usr/bin/python

import sys

if len(sys.argv) != 3:
  print "Usage: tokens2classes.py source_classes target_classes"
  exit(1)

source_to_topics = {}
for line in open(sys.argv[1],'r'):
  term,cls = line.split()
  source_to_topics[term] = cls

target_to_topics = {}
for line in open(sys.argv[2],'r'):
  term,cls = line.split()
  target_to_topics[term] = cls

for line in sys.stdin:
  source, target, tail = line.split(" ||| ")

  for token in source.split():
    print source_to_topics[token],
  print "|||",
  for token in target.split():
    print target_to_topics[token],
  print "|||", tail,
