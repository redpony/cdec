#!/usr/bin/env python
import sys
import codecs
import os
import os.path
from xml.sax.saxutils import escape

graPrefix = sys.argv[1]

# Second argument can be a file with observable sentence-level features,
# one set of features per line (parallel with source sentences). Features are space-delimited indicator features.
obsFeatsFile = None
if len(sys.argv) == 3:
  obsFeatsFilename = sys.argv[2]
  obsFeatsFile = open(obsFeatsFilename)

sys.stdin = codecs.getreader("utf-8")(sys.stdin)
sys.stdout = codecs.getwriter("utf-8")(sys.stdout)

i = 0
for line in sys.stdin:
  filename = "%s%d"%(graPrefix,i)
  if not os.path.exists(filename):
    filenameGz = filename + ".gz"
    if not os.path.exists(filenameGz):
      print >>sys.stderr, "Grammar file not found: ", filename, filenameGz
      sys.exit(1)
    else:
      filename = filenameGz
    
  if obsFeatsFile:
    obsFeats = obsFeatsFile.next().strip()
    print '<seg id="%d" features="%s" grammar="%s"> '%(i,obsFeats,filename) + escape(line.strip()) + " </seg>"
  else:
    print '<seg id="%d" grammar="%s"> '%(i,filename) + escape(line.strip()) + " </seg>"
  i+=1

