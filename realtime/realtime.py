#!/usr/bin/env python

import argparse
import logging
import sys
import threading
import time

import rt

ABOUT = '''Realtime adaptive translation with cdec (See README.md)

Code by Michael Denkowski

Citation:
@InProceedings{realtime,
    author       = {Michael Denkowski and Chris Dyer and Alon Lavie},
    title        = {Learning from Post-Editing: Online Model Adaptation for Statistical Machine Translation},
    booktitle    = {Proceedings of the 14th Conference of the European Chapter of the Association for Computational Linguistics}
    year         = {2014},
}

'''

class Parser(argparse.ArgumentParser):

    def error(self, message):
        sys.stderr.write(ABOUT)
        self.print_help()
        sys.stderr.write('\n{}\n'.format(message))
        sys.exit(2)

def handle_line(translator, line, output, ctx_name):
    res = translator.command_line(line, ctx_name)
    if res:
        output.write('{}\n'.format(res))
        output.flush()

def test1(translator, input, output, ctx_name):
    inp = open(input)
    out = open(output, 'w')
    for line in inp:
        handle_line(translator, line.strip(), out, ctx_name)
    out.close()

def debug(translator, input):
    # Test 1: multiple contexts
    threads = []
    for i in range(4):
        t = threading.Thread(target=test1, args=(translator, input, '{}.out.{}'.format(input, i), str(i)))
        threads.append(t)
        t.start()
        time.sleep(30)
    # Test 2: flood
    out = open('{}.out.flood'.format(input), 'w')
    inp = open(input)
    while True:
        line = inp.readline()
        if not line:
            break
        line = line.strip()
        t = threading.Thread(target=handle_line, args=(translator, line.strip(), out, None))
        threads.append(t)
        t.start()
        time.sleep(1) 
    translator.drop_ctx(None)
    # Join test threads
    for t in threads:
        t.join()

def main():

    parser = Parser()
    parser.add_argument('-c', '--config', required=True, help='Config directory')
    parser.add_argument('-s', '--state', help='Load state file to default context (saved incremental data)')
    parser.add_argument('-n', '--normalize', help='Normalize text (tokenize, translate, detokenize)', action='store_true')
    parser.add_argument('-T', '--temp', help='Temp directory (default /tmp)', default='/tmp')
    parser.add_argument('-a', '--cache', help='Grammar cache size (default 5)', default='5')
    parser.add_argument('-v', '--verbose', help='Info to stderr', action='store_true')
    parser.add_argument('-D', '--debug-test', help='Run debug tests on input file')
    args = parser.parse_args()

    if args.verbose:
        logging.basicConfig(level=logging.INFO)

    with rt.RealtimeTranslator(args.config, tmpdir=args.temp, cache_size=int(args.cache), norm=args.normalize) as translator:

        # Debugging
        if args.debug_test:
            debug(translator, args.debug_test)
            return

        # Load state if given
        if args.state:
            rtd.load_state(state)

        # Read lines and commands
        while True:
            line = sys.stdin.readline()
            if not line:
                break
            line = line.strip()
            res = translator.command_line(line)
            if res:
                sys.stdout.write('{}\n'.format(res))
                sys.stdout.flush()
     
if __name__ == '__main__':
    main()
