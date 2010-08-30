#!/usr/bin/python

import sys
import gzip

#usage: morfsegment.py inputvocab.gz segmentation.ready
#  stdin: the data to segment
#  stdout: the segmented data

if len(sys.argv) < 3:
  print "usage: morfsegment.py inputvocab.gz segmentation.ready [marker]"
  print "  stdin: the data to segment"
  print "  stdout: the segmented data"
  sys.exit()

#read index:
split_index={}

marker="##"

if len(sys.argv) > 3:
  marker=sys.argv[3]

word_vocab=gzip.open(sys.argv[1], 'rb') #inputvocab.gz
seg_vocab=open(sys.argv[2], 'r') #segm.ready..

for seg in seg_vocab:
  #seg = ver# #wonder\n
  #wordline = 1 verwonder\n
  word = word_vocab.readline().strip().split(' ')
  assert(len(word) == 2)
  word = word[1]
  seg=seg.strip()

  if seg != word:
    split_index[word] = seg

word_vocab.close()
seg_vocab.close()

for line in sys.stdin:
  words = line.strip().split()

  newsent = []
  for word in words:
    splitword = split_index.get(word, word)
    newsent.append(splitword)

  print ' '.join(newsent)

