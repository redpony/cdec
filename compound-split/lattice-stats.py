#!/usr/bin/python

import sys
import optparse
from heapq import heappush, heappop
import math

lc = 0
maxdepth = -1
total_depth = 0
total_cols = 0
total_paths = 0.0

optparser = optparse.OptionParser()
optparser.add_option("-k", "--k-best", dest="k", type='int', help="number of best paths", default=1)
(opts,args) = optparser.parse_args()
n = opts.k

if len(args)==0: args=(sys.stdin)

for fname in args:
  if (type(fname) == type('')):
    f = open(fname, "r")
  else:
    f = fname
  lc = 0
  nodes   = 0
  for line in f:
    lc+=1
    cn = eval(line)
    if (len(cn[0]) == 0):
      continue

    paths = 1.0
    for col in cn:
      depth=len(col)
      paths*=float(depth)
      nodes += depth
      total_depth += depth
      total_cols += 1
    total_paths+=paths
  avg=float(total_depth)/float(lc)
  print "averagePaths=%g" % (total_paths / float(lc))
  print "averageNodes=%f" % (float(total_depth) / float(lc))
  print "totalPaths=%f" % (float(total_depth))
  print "Nodes/Len=%f" % (float(total_depth)/float(total_cols))
  print "averageLen=%f" % (float(total_cols) / float(lc))
