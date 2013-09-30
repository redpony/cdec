#!/usr/bin/env python

import itertools
import sys

def main():

    if len(sys.argv[1:]) < 2:
        sys.stderr.write('usage: {} test.src test.ref [ctx_name] >test.input\n'.format(sys.argv[0]))
        sys.exit(2)

    ctx_name = ' {}'.format(sys.argv[3]) if len(sys.argv[1:]) > 2 else ''
    for (src, ref) in itertools.izip(open(sys.argv[1]), open(sys.argv[2])):
        sys.stdout.write('TR{} ||| {}'.format(ctx_name, src))
        sys.stdout.write('LEARN{} ||| {} ||| {}'.format(ctx_name, src.strip(), ref))

if __name__ == '__main__':
    main()
