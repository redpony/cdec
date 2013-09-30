#!/usr/bin/env python

import itertools
import sys

def main():

    if len(sys.argv[1:]) != 2:
        sys.stderr.write('usage: {} test.src test.ref >test.input\n'.format(sys.argv[0]))
        sys.exit(2)

    for (src, ref) in itertools.izip(open(sys.argv[1]), open(sys.argv[2])):
        sys.stdout.write('TR ||| {}'.format(src))
        sys.stdout.write('LEARN ||| {} ||| {}'.format(src.strip(), ref))

if __name__ == '__main__':
    main()
