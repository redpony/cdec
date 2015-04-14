#!/usr/bin/env python

import gzip
import math
import os
import shutil
import subprocess
import sys
import tempfile

DEFAULT_JOBS = 8
DEFAULT_TMP = '/tmp'

TOKENIZER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'tokenize-anything.sh')

def gzopen(f):
    return gzip.open(f) if f.endswith('.gz') else open(f)

def wc(f):
    return sum(1 for line in gzopen(f))

def main(argv):

    if len(argv[1:]) < 1:
        sys.stderr.write('Parallelize text normalization with multiple instances of tokenize-anything.sh\n\n')
        sys.stderr.write('Usage: {} in-file [jobs [temp-dir]] >out-file\n'.format(argv[0]))
        sys.exit(2)

    in_file = argv[1]
    jobs = int(argv[2]) if len(argv[1:]) > 1 else DEFAULT_JOBS
    tmp = argv[3] if len(argv[1:]) > 2 else DEFAULT_TMP

    work = tempfile.mkdtemp(prefix='tok.', dir=tmp)
    in_wc = wc(in_file)
    # Don't start more jobs than we have lines
    jobs = min(jobs, in_wc)
    lines_per = int(math.ceil(float(in_wc)/jobs))

    inp = gzopen(in_file)
    procs = []
    files = []
    outs = []
    for i in range(jobs):
        raw = os.path.join(work, 'in.{}'.format(i))
        tok = os.path.join(work, 'out.{}'.format(i))
        files.append(tok)
        # Write raw batch
        raw_out = open(raw, 'w')
        for _ in range(lines_per):
            line = inp.readline()
            if not line:
                break
            raw_out.write(line)
        raw_out.close()
        # Start tokenizer
        raw_in = open(raw)
        tok_out = open(tok, 'w')
        outs.append(tok_out)
        p = subprocess.Popen(TOKENIZER, stdin=raw_in, stdout=tok_out)
        procs.append(p)

    # Cat output of each tokenizer as it finishes
    for (p, f, o) in zip(procs, files, outs):
        p.wait()
        o.close()
        for line in open(f):
            sys.stdout.write(line)

    # Cleanup
    shutil.rmtree(work)

if __name__ == '__main__':
    main(sys.argv)
