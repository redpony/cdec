#!/usr/bin/env python

import os
import sys
import subprocess

def main():

    if len(sys.argv[1:]) != 4:
        sys.stderr.write('run:\n')
        sys.stderr.write('  fast_align -i corpus.f-e -d -v -o -p fwd_params >fwd_align 2>fwd_err\n')
        sys.stderr.write('  fast_align -i corpus.f-e -r -d -v -o -p rev_params >rev_align 2>rev_err\n')
        sys.stderr.write('\n')
        sys.stderr.write('then run:\n')
        sys.stderr.write('  {} fwd_params fwd_err rev_params rev_err <in.f-e >out.f-e.gdfa\n'.format(sys.argv[0]))
        sys.exit(2)

    (f_p, f_err, r_p, r_err) = sys.argv[1:]
    
    (f_T, f_m) = find_Tm(f_err)
    (r_T, r_m) = find_Tm(r_err)
    
    fast_align = os.path.join(os.path.dirname(__file__), 'fast_align')
    f_cmd = [fast_align, '-i', '-', '-d', '-T', f_T, '-m', f_m, '-f', f_p]
    r_cmd = [fast_align, '-i', '-', '-d', '-T', r_T, '-m', r_m, '-f', r_p, '-r']

    atools = os.path.join(os.path.dirname(os.path.dirname(__file__)), 'utils', 'atools')
    tools_cmd = [atools, '-i', '-', '-j', '-', '-c', 'grow-diag-final-and']

    sys.stderr.write('running: {}\n'.format(' '.join(f_cmd)))
    sys.stderr.write('running: {}\n'.format(' '.join(r_cmd)))
    sys.stderr.write('running: {}\n'.format(' '.join(tools_cmd)))

    f_a = subprocess.Popen(f_cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    r_a = subprocess.Popen(r_cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    tools = subprocess.Popen(tools_cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE)

    for line in sys.stdin:
        f_a.stdin.write(line)
        r_a.stdin.write(line)
        # f words ||| e words ||| links ||| score
        f_line = f_a.stdout.readline().split('|||')[2].strip()
        r_line = r_a.stdout.readline().split('|||')[2].strip()
        tools.stdin.write('{}\n'.format(f_line))
        tools.stdin.write('{}\n'.format(r_line))
        sys.stdout.write(tools.stdout.readline())

def find_Tm(err):
    (T, m) = ('', '')
    for line in open(err):
        # expected target length = source length * N
        if 'expected target length' in line:
            m = line.split()[-1]
        elif 'final tension' in line:
            T = line.split()[-1]
    return (T, m)

if __name__ == '__main__':
    main()
