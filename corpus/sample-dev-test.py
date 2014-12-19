#!/usr/bin/env python

import gzip
import os
import sys

HELP = '''Process an input corpus by dividing it into pseudo-documents and uniformly
sampling train, dev, and test sets (simulate uniform sampling at the document
level when document boundaries are unknown)

usage: {} in_file out_prefix doc_size dev_test_docs [-lc]
recommended: doc_size=20, dev_test_docs=100
'''

def gzopen(f):
    return gzip.open(f, 'rb') if f.endswith('.gz') else open(f, 'r')

def wc(f):
    return sum(1 for _ in gzopen(f))

def main(argv):
    
    if len(argv[1:]) < 4:
        sys.stderr.write(HELP.format(os.path.basename(argv[0])))
        sys.exit(2)
    
    in_file = os.path.abspath(argv[1])
    out_prefix = os.path.abspath(argv[2])
    doc_size = int(argv[3])
    dev_test_docs = int(argv[4])
    lc = (len(argv[1:]) == 5 and argv[5] == '-lc')

    corpus_size = wc(in_file)
    total_docs = corpus_size / doc_size
    leftover = corpus_size % doc_size
    train_docs = total_docs - (2 * dev_test_docs)
    train_batch_size = (train_docs / dev_test_docs) - 2

    sys.stderr.write('Splitting {} lines ({} documents)\n'.format(corpus_size, total_docs + (1 if leftover else 0)))
    sys.stderr.write('Train: {} ({})\n'.format((train_docs * doc_size) + leftover, train_docs + (1 if leftover else 0)))
    sys.stderr.write('Dev: {} ({})\n'.format(dev_test_docs * doc_size, dev_test_docs))
    sys.stderr.write('Test: {} ({})\n'.format(dev_test_docs * doc_size, dev_test_docs))

    with gzopen(in_file) as inp, \
            open('{}.train'.format(out_prefix), 'w') as train_out, \
            open('{}.dev'.format(out_prefix), 'w') as dev_out, \
            open('{}.test'.format(out_prefix), 'w') as test_out:
        i = 0
        for _ in range(dev_test_docs):
            for _ in range(train_batch_size):
                for _ in range(doc_size):
                    i += 1
                    train_out.write('{} ||| {}'.format(i, inp.readline()) if lc else inp.readline())
            for _ in range(doc_size):
                i += 1
                dev_out.write('{} ||| {}'.format(i, inp.readline()) if lc else inp.readline())
            for _ in range(doc_size):
                i += 1
                test_out.write('{} ||| {}'.format(i, inp.readline()) if lc else inp.readline())
        for line in inp:
            i += 1
            train_out.write('{} ||| {}'.format(i, line) if lc else line)

if __name__ == '__main__':
    main(sys.argv)
