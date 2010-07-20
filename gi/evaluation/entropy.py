#!/usr/bin/env python

import sys, math, itertools, getopt

def usage():
    print >>sys.stderr, 'Usage:', sys.argv[0], '[-s slash_threshold] input file'
    sys.exit(0)

optlist, args = getopt.getopt(sys.argv[1:], 'hs:')
slash_threshold = None
for opt, arg in optlist:
    if opt == '-s':
        slash_threshold = int(arg)
    else:
        usage()
if len(args) != 1:
    usage()

infile = open(args[0])
N = 0
frequencies = {}

for line in infile:

    for part in line.split('||| ')[1].split():
        tag = part.split(':',1)[1]

        if slash_threshold == None or tag.count('/') + tag.count('\\') <= slash_threshold:
            frequencies.setdefault(gtag, 0)
            frequencies[gtag] += 1
            N += 1

h = 0
for tag, c in frequencies.items():
    h -= c * (math.log(c, 2) - math.log(N, 2))
h /= N

print 'entropy', h
