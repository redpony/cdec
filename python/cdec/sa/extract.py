#!/usr/bin/env python
import sys
import os
import re
import gzip
import argparse
import logging
import signal
import multiprocessing as mp
import cdec.sa
from cdec.sa._sa import monitor_cpu

extractor, prefix = None, None
online, compress = False, False

def make_extractor(args):
    global extractor, prefix, online, compress
    signal.signal(signal.SIGINT, signal.SIG_IGN) # Let parent process catch Ctrl+C
    load_features(args.features)
    extractor = cdec.sa.GrammarExtractor(args.config, online)
    prefix = args.grammars
    online = args.online
    compress = args.compress

def load_features(features):
    for featdef in features:
        logging.info('Loading additional feature definitions from %s', featdef)
        prefix = os.path.dirname(featdef)
        sys.path.append(prefix)
        __import__(os.path.basename(featdef).replace('.py', ''))
        sys.path.remove(prefix)

def extract(inp):
    global extractor, prefix, online, compress
    i, sentence = inp
    sentence = sentence[:-1]
    fields = re.split('\s*\|\|\|\s*', sentence)
    suffix = ''
    # 3 fields for online mode, 1 for normal
    if online:
        if len(fields) < 3:
            sys.stderr.write('Error: online mode requires references and alignments.'
                    '  Not adding sentence to training data: {}\n'.format(sentence))
            sentence = fields[0]
        else:
            sentence, reference, alignment = fields[0:3]
        if len(fields) > 3:
            suffix = ' ||| ' + ' ||| '.join(fields[3:])
    else:
        if len(fields) > 1:
            sentence = fields[0]
            suffix = ' ||| ' + ' ||| '.join(fields[1:])

    grammar_file = os.path.join(prefix, 'grammar.'+str(i))
    if compress: grammar_file += '.gz'
    with (gzip.open if compress else open)(grammar_file, 'w') as output:
        for rule in extractor.grammar(sentence):
            output.write(str(rule)+'\n')
    # Add training instance _after_ extracting grammars
    if online:
        extractor.add_instance(sentence, reference, alignment)
    grammar_file = os.path.abspath(grammar_file)
    return '<seg grammar="{}" id="{}">{}</seg>{}'.format(grammar_file, i, sentence, suffix)

def main():
    global online
    logging.basicConfig(level=logging.INFO)
    parser = argparse.ArgumentParser(description='Extract grammars from a compiled corpus.')
    parser.add_argument('-c', '--config', required=True,
                        help='extractor configuration')
    parser.add_argument('-g', '--grammars', required=True,
                        help='grammar output path')
    parser.add_argument('-j', '--jobs', type=int, default=1,
                        help='number of parallel extractors')
    parser.add_argument('-s', '--chunksize', type=int, default=10,
                        help='number of sentences / chunk')
    parser.add_argument('-f', '--features', nargs='*', default=[],
                        help='additional feature definitions')
    parser.add_argument('-o', '--online', action='store_true',
                        help='online grammar extraction')
    parser.add_argument('-z', '--compress', action='store_true',
                        help='compress grammars with gzip')
    args = parser.parse_args()

    if not os.path.exists(args.grammars):
        os.mkdir(args.grammars)
    for featdef in args.features:
        if not featdef.endswith('.py'):
            sys.stderr.write('Error: feature definition file <{}>'
                    ' should be a python module\n'.format(featdef))
            sys.exit(1)

    online = args.online

    start_time = monitor_cpu()
    if args.jobs > 1:
        logging.info('Starting %d workers; chunk size: %d', args.jobs, args.chunksize)
        pool = mp.Pool(args.jobs, make_extractor, (args,))
        try:
            for output in pool.imap(extract, enumerate(sys.stdin), args.chunksize):
                print(output)
        except KeyboardInterrupt:
            pool.terminate()
    else:
        make_extractor(args)
        for output in map(extract, enumerate(sys.stdin)):
            print(output)

    stop_time = monitor_cpu()
    logging.info("Overall extraction step took %f seconds", stop_time - start_time)

if __name__ == '__main__':
    main()
