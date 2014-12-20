#!/usr/bin/env python

import gzip
import os
import sys

HELP = '''Process an input corpus by dividing it into pseudo-documents and uniformly
sampling train and dev sets (simulate uniform sampling at the document level
when document boundaries are unknown)

usage: {} in_file out_prefix doc_size docs_per_dev_set dev_sets [-lc]
recommended: doc_size=20, docs_per_dev_set=100, dev_sets=2 (dev and test)
'''

def gzopen(f):
    return gzip.open(f, 'rb') if f.endswith('.gz') else open(f, 'r')

def wc(f):
    return sum(1 for _ in gzopen(f))

def main(argv):
    
    if len(argv[1:]) < 5:
        sys.stderr.write(HELP.format(os.path.basename(argv[0])))
        sys.exit(2)
    
    # Args
    in_file = os.path.abspath(argv[1])
    out_prefix = os.path.abspath(argv[2])
    doc_size = int(argv[3])
    docs_per_dev_set = int(argv[4])
    dev_sets = int(argv[5])
    lc = (len(argv[1:]) == 6 and argv[6] == '-lc')

    # Compute sizes
    corpus_size = wc(in_file)
    total_docs = corpus_size / doc_size
    leftover = corpus_size % doc_size
    train_docs = total_docs - (dev_sets * docs_per_dev_set)
    train_batch_size = (train_docs / docs_per_dev_set)

    # Report
    sys.stderr.write('Splitting {} lines ({} documents)\n'.format(corpus_size, total_docs + (1 if leftover else 0)))
    sys.stderr.write('Train: {} ({})\n'.format((train_docs * doc_size) + leftover, train_docs + (1 if leftover else 0)))
    sys.stderr.write('Dev: {} x {} ({})\n'.format(dev_sets, docs_per_dev_set * doc_size, docs_per_dev_set))

    inp = gzopen(in_file)
    train_out = open('{}.train'.format(out_prefix), 'w')
    dev_out = [open('{}.dev.{}'.format(out_prefix, i + 1), 'w') for i in range(dev_sets)]
    i = 0

    # For each set of documents
    for _ in range(docs_per_dev_set):
        # Write several documents to train
        for _ in range(train_batch_size):
            for _ in range(doc_size):
                i += 1
                train_out.write('{} ||| {}'.format(i, inp.readline()) if lc else inp.readline())
        # Write a document to each dev
        for out in dev_out:
            for _ in range(doc_size):
                i += 1
                out.write('{} ||| {}'.format(i, inp.readline()) if lc else inp.readline())
    # Write leftover lines to train
    for line in inp:
        i += 1
        train_out.write('{} ||| {}'.format(i, line) if lc else line)

    train_out.close()
    for out in dev_out:
        out.close()

if __name__ == '__main__':
    main(sys.argv)
