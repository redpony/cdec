#!/usr/bin/env python

import os
import sys

def main():

    if len(sys.argv[1:]) != 4:
        sys.stderr.write('run:\n')
        sys.stderr.write('  fast_align -i corpus.f-e -d -v -o -p fwd_params >fwd_align 2>fwd_err\n')
        sys.stderr.write('  fast_align -i corpus.f-e -r -d -v -o -p rev_params >rev_align 2>rev_err\n')
        sys.stderr.write('\n')
        sys.stderr.write('then run:\n')
        sys.stderr.write('  {} fwd_params fwd_err rev_params rev_err <in.f-e >out.f-e.gdfa\n'.format(sys.argv[0]))
        sys.exit(2)

    # Hook into realtime
    sys.path.append(os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'realtime'))
    from rt import ForceAligner

    aligner = ForceAligner(*sys.argv[1:])
    
    for line in sys.stdin:
        sys.stdout.write('{}\n'.format(aligner.align(line.strip())))

    aligner.close()
    
if __name__ == '__main__':
    main()
