import logging
import os
import sys
import subprocess
import threading

import util

logger = logging.getLogger('rt.aligner')

class ForceAligner:

    def __init__(self, fwd_params, fwd_err, rev_params, rev_err, heuristic='grow-diag-final-and'):

        cdec_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
        fast_align = os.path.join(cdec_root, 'word-aligner', 'fast_align')
        atools = os.path.join(cdec_root, 'utils', 'atools')

        (fwd_T, fwd_m) = self.read_err(fwd_err)
        (rev_T, rev_m) = self.read_err(rev_err)

        fwd_cmd = [fast_align, '-i', '-', '-d', '-T', fwd_T, '-m', fwd_m, '-f', fwd_params]
        rev_cmd = [fast_align, '-i', '-', '-d', '-T', rev_T, '-m', rev_m, '-f', rev_params, '-r']
        tools_cmd = [atools, '-i', '-', '-j', '-', '-c', heuristic]

        logger.info('Executing: {}'.format(' '.join(fwd_cmd)))
        self.fwd_align = util.popen_io(fwd_cmd)

        logger.info('Executing: {}'.format(' '.join(rev_cmd)))
        self.rev_align = util.popen_io(rev_cmd)

        logger.info('Executing: {}'.format(' '.join(tools_cmd)))
        self.tools = util.popen_io(tools_cmd)

        # Used to guarantee thread safety
        self.lock = util.FIFOLock()

    def align(self, source, target):
        '''Threadsafe, FIFO'''
        return self.align_formatted('{} ||| {}'.format(source, target))

    def align_formatted(self, line):
        '''Threadsafe, FIFO'''
        self.lock.acquire()
        self.fwd_align.stdin.write('{}\n'.format(line))
        self.rev_align.stdin.write('{}\n'.format(line))
        # f words ||| e words ||| links ||| score
        fwd_line = self.fwd_align.stdout.readline().split('|||')[2].strip()
        rev_line = self.rev_align.stdout.readline().split('|||')[2].strip()
        self.tools.stdin.write('{}\n'.format(fwd_line))
        self.tools.stdin.write('{}\n'.format(rev_line))
        al_line = self.tools.stdout.readline().strip()
        self.lock.release()
        return al_line
 
    def close(self, force=False):
        if not force:
            self.lock.acquire()
        self.fwd_align.stdin.close()
        self.fwd_align.wait()
        self.rev_align.stdin.close()
        self.rev_align.wait()
        self.tools.stdin.close()
        self.tools.wait()
        if not force:
            self.lock.release()

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
