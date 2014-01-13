#!/usr/bin/env python

import os
import sys

# Hook into realtime
sys.path.append(os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'realtime', 'rt'))
from aligner import ForceAligner

def main():

    if len(sys.argv[1:]) < 4:
        sys.stderr.write('run:\n')
        sys.stderr.write('  fast_align -i corpus.f-e -d -v -o -p fwd_params >fwd_align 2>fwd_err\n')
        sys.stderr.write('  fast_align -i corpus.f-e -r -d -v -o -p rev_params >rev_align 2>rev_err\n')
        sys.stderr.write('\n')
        sys.stderr.write('then run:\n')
        sys.stderr.write('  {} fwd_params fwd_err rev_params rev_err [heuristic] <in.f-e >out.f-e.gdfa\n'.format(sys.argv[0]))
        sys.stderr.write('\n')
        sys.stderr.write('where heuristic is one of: (intersect union grow-diag grow-diag-final grow-diag-final-and) default=grow-diag-final-and\n')
        sys.exit(2)

    aligner = ForceAligner(*sys.argv[1:])

    while True:
        line = sys.stdin.readline()
        if not line:
            break
        sys.stdout.write('{}\n'.format(aligner.align_formatted(line.strip())))
        sys.stdout.flush()

    aligner.close()
    
if __name__ == '__main__':
    main()
