#!/usr/bin/python

import sys

#linestripper   file file maxlen [numlines]

if len(sys.argv) < 3:
  print "linestripper   file1 file2 maxlen [numlines]" 
  print " outputs subset of file1 to stdout, ..of file2 to stderr"
  sys.exit(1)


f1 = open(sys.argv[1],'r')
f2 = open(sys.argv[2],'r')

maxlen=int(sys.argv[3])
numlines = 0

if len(sys.argv) > 4:
  numlines = int(sys.argv[4])

count=0
for line1 in f1:
  line2 = f2.readline()
  
  w1 = len(line1.strip().split())
  w2 = len(line2.strip().split())

  if w1 <= maxlen and w2 <= maxlen:
    count = count + 1
    sys.stdout.write(line1)
    sys.stderr.write(line2)
 
  if numlines > 0 and count >= numlines:
    break

f1.close()
f2.close()
  

