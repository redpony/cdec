#!/usr/bin/env python
import sys

featNames = [ line.strip() for line in open(sys.argv[1]) if not line.startswith('#') ]

for line in sys.stdin:
  try:
    (lhs, src, tgt, feats, align) = line.strip("\n").split(' ||| ')
  except:
    print >>sys.stderr, 'WARNING: No alignments:', line
    try:
      (lhs, src, tgt, feats) = line.strip().split(' ||| ')
      align = ''
    except:
      print >>sys.stderr, "ERROR: Malformed line:", line
      raise
  featValues = feats.split(' ')
  namedFeats = ' '.join( name+"="+value for (name, value) in zip(featNames, featValues) )
  print " ||| ".join( (lhs, src, tgt, namedFeats, align) )
