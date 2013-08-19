import os
import sys
import subprocess

import util

class ForceAligner:

    def __init__(self, fwd_params, fwd_err, rev_params, rev_err):

        cdec_root = os.path.dirname(os.path.dirname(os.path.dirname(__file__)))
        fast_align = os.path.join(cdec_root, 'word-aligner', 'fast_align')
        atools = os.path.join(cdec_root, 'utils', 'atools')

        (fwd_T, fwd_m) = self.read_err(fwd_err)
        (rev_T, rev_m) = self.read_err(rev_err)

        fwd_cmd = [fast_align, '-i', '-', '-d', '-T', fwd_T, '-m', fwd_m, '-f', fwd_params]
        rev_cmd = [fast_align, '-i', '-', '-d', '-T', rev_T, '-m', rev_m, '-f', rev_params, '-r']
        tools_cmd = [atools, '-i', '-', '-j', '-', '-c', 'grow-diag-final-and']

        self.fwd_align = util.popen_io(fwd_cmd)
        self.rev_align = util.popen_io(rev_cmd)
        self.tools = util.popen_io(tools_cmd)

    def align(self, line):
        self.fwd_align.stdin.write('{}\n'.format(line))
        self.rev_align.stdin.write('{}\n'.format(line))
        # f words ||| e words ||| links ||| score
        fwd_line = self.fwd_align.stdout.readline().split('|||')[2].strip()
        rev_line = self.rev_align.stdout.readline().split('|||')[2].strip()
        self.tools.stdin.write('{}\n'.format(fwd_line))
        self.tools.stdin.write('{}\n'.format(rev_line))
        return self.tools.stdout.readline().strip()
 
    def close(self):
        self.fwd_align.stdin.close()
        self.rev_align.stdin.close()
        self.tools.stdin.close()

    def read_err(self, err):
        (T, m) = ('', '')
        for line in open(err):
            # expected target length = source length * N
            if 'expected target length' in line:
                m = line.split()[-1]
            # final tension: N
            elif 'final tension' in line:
                T = line.split()[-1]
        return (T, m)
