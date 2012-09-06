#!/usr/bin/env python
import sys
import os
import argparse
import logging
import multiprocessing as mp
import signal
import cdec.sa

extractor, prefix = None, None
def make_extractor(config, grammars, features):
    global extractor, prefix
    signal.signal(signal.SIGINT, signal.SIG_IGN) # Let parent process catch Ctrl+C
    load_features(features)
    extractor = cdec.sa.GrammarExtractor(config)
    prefix = grammars

def load_features(features):
    for featdef in features:
        logging.info('Loading additional feature definitions from %s', featdef)
        prefix = os.path.dirname(featdef)
        sys.path.append(prefix)
        __import__(os.path.basename(featdef).replace('.py', ''))
        sys.path.remove(prefix)

def extract(inp):
    global extractor, prefix
    i, sentence = inp
    sentence = sentence[:-1]
    grammar_file = os.path.join(prefix, 'grammar.{0}'.format(i))
    with open(grammar_file, 'w') as output:
        for rule in extractor.grammar(sentence):
            output.write(str(rule)+'\n')
    grammar_file = os.path.abspath(grammar_file)
    return '<seg grammar="{0}" id="{1}">{2}</seg>'.format(grammar_file, i, sentence)

def main():
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
    args = parser.parse_args()

    if not os.path.exists(args.grammars):
        os.mkdir(args.grammars)
    for featdef in args.features:
        if not featdef.endswith('.py'):
            sys.stderr.write('Error: feature definition file <{0}>'
                    ' should be a python module\n'.format(featdef))
            sys.exit(1)
    
    if args.jobs > 1:
        logging.info('Starting %d workers; chunk size: %d', args.jobs, args.chunksize)
        pool = mp.Pool(args.jobs, make_extractor, (args.config, args.grammars, args.features))
        try:
            for output in pool.imap(extract, enumerate(sys.stdin), args.chunksize):
                print(output)
        except KeyboardInterrupt:
            pool.terminate()
    else:
        make_extractor(args.config, args.grammars, args.features)
        for output in map(extract, enumerate(sys.stdin)):
            print(output)

if __name__ == '__main__':
    main()
