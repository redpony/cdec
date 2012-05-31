#!/usr/bin/env python
import sys

(numRefs, outPrefix) = sys.argv[1:]
numRefs = int(numRefs)

outs = [open(outPrefix+str(i), "w") for i in range(numRefs)]

i = 0
for line in sys.stdin:
  outs[i].write(line)
  i = (i + 1) % numRefs

for out in outs:
  out.close()
