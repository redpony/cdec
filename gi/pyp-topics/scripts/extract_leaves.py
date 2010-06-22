#!/usr/bin/python

import nltk
import nltk.probability
import sys
import getopt 

lexicalise=False
rm_traces=False
cutoff=100
length_cutoff=10000
try:                                
  opts, args = getopt.getopt(sys.argv[1:], "hs:c:l", ["help", "lexicalise", "cutoff","sentence-length","remove-traces"])
except getopt.GetoptError:          
  print "Usage: extract_leaves.py [-lsc]"                        
  sys.exit(2)                     
for opt, arg in opts:                
  if opt in ("-h", "--help"):      
    print "Usage: extract_leaves.py [-lsc]"                        
    sys.exit()                  
  elif opt in ("-l", "--lexicalise"):                
    lexicalise = True                 
  elif opt in ("-c", "--cutoff"):                
    cutoff = int(arg) 
  elif opt in ("-s", "--sentence-length"):                
    length_cutoff = int(arg) 
  elif opt in ("--remove-traces"):                
    rm_traces = True                 

token_freq = nltk.probability.FreqDist()
lines = []
for line in sys.stdin:
  t = nltk.Tree.parse(line)
  pos = t.pos()
  if len(pos) <= length_cutoff:
    lines.append(pos)
    for token, tag in pos:
      token_freq.inc(token)  

for line in lines:
  for token,tag in line:
    if not (rm_traces and tag == "-NONE-"):
      if lexicalise:
        if token_freq[token] < cutoff:
          token = '-UNK-'
        print '%s|%s' % (token,tag),
      else:
        print '%s' % tag,
  print
