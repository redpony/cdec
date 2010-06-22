#!/usr/bin/python

import sys
from collections import defaultdict

def dict_max(d):
  max_val=-1
  max_key=None
  for k in d:
    if d[k] > max_val: 
      max_val = d[k]
      max_key = k
  assert max_key
  return max_key

if len(sys.argv) != 3:
  print "Usage: score-mkcls.py gold classes"
  exit(1)

gold_file=open(sys.argv[1],'r')

term_to_topics = {}
for line in open(sys.argv[2],'r'):
  term,cls = line.split()
  term_to_topics[term] = cls

gold_to_topics = defaultdict(dict)
topics_to_gold = defaultdict(dict)

for gold_line in gold_file:
  gold_tokens = gold_line.split()
  for gold_token in gold_tokens:
    gold_term,gold_tag = gold_token.rsplit('|',1)
    pred_token = term_to_topics[gold_term]
    gold_to_topics[gold_tag][pred_token] \
      = gold_to_topics[gold_tag].get(pred_token, 0) + 1
    topics_to_gold[pred_token][gold_tag] \
      = topics_to_gold[pred_token].get(gold_tag, 0) + 1

pred=0
correct=0
gold_file=open(sys.argv[1],'r')
for gold_line in gold_file:
  gold_tokens = gold_line.split()

  for gold_token in gold_tokens:
    gold_term,gold_tag = gold_token.rsplit('|',1)
    pred_token = term_to_topics[gold_term]
    print "%s|%s|%s" % (gold_token, pred_token, dict_max(topics_to_gold[pred_token])),
    pred += 1
    if gold_tag == dict_max(topics_to_gold[pred_token]):
      correct += 1
  print
print >>sys.stderr, "Many-to-One Accuracy = %f" % (float(correct) / pred)
#for x in gold_to_topics: 
#  print x,dict_max(gold_to_topics[x])
#print "###################################################"
#for x in range(len(topics_to_gold)): 
#  print x,dict_max(topics_to_gold[str(x)])
#  print x,topics_to_gold[str(x)]
#print term_to_topics
