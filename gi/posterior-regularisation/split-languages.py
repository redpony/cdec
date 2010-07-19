#!/usr/bin/python

import sys

sout = open(sys.argv[1], 'w')
tout = open(sys.argv[2], 'w')
for line in sys.stdin:
	phrase, contexts = line.rstrip().split('\t')
	sp, tp = phrase.split(' <SPLIT> ')
	sout.write('%s\t' % sp)
	tout.write('%s\t' % tp)
	parts = contexts.split(' ||| ')
	for i in range(0, len(parts), 2):
		sc, tc = parts[i].split(' <SPLIT> ')
		if i != 0:
			sout.write(' ||| ')
			tout.write(' ||| ')
		sout.write('%s ||| %s' % (sc, parts[i+1]))
		tout.write('%s ||| %s' % (tc, parts[i+1]))
	sout.write('\n')
	tout.write('\n')
sout.close()
tout.close()
