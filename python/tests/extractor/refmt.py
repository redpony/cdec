#!/usr/bin/env python

import collections, sys

lines = []
f = collections.defaultdict(int)
fe = collections.defaultdict(lambda: collections.defaultdict(int))

for line in sys.stdin:
    tok = [x.strip() for x in line.split('|||')]
    count = int(tok[4])
    f[tok[1]] += count
    fe[tok[1]][tok[2]] += count
    lines.append(tok)

for tok in lines:
    feat = 'IsSingletonF={0}.0 IsSingletonFE={1}.0'.format(
        0 if f[tok[1]] > 1 else 1,
        0 if fe[tok[1]][tok[2]] > 1 else 1)
    print ' ||| '.join((tok[0], tok[1], tok[2], feat, tok[3]))
