#!/usr/bin/env python

import argparse
import logging
import signal
import sys
import threading

import rt

class Parser(argparse.ArgumentParser):

    def error(self, message):
        self.print_help()
        sys.stderr.write('\n{}\n'.format(message))
        sys.exit(2)

def main():

    parser = Parser(description='Real-time adaptive translation with cdec.  (See README.md)')
    parser.add_argument('-c', '--config', required=True, help='Config directory')
    parser.add_argument('-s', '--state', help='Load state file (saved incremental data)')
    parser.add_argument('-n', '--normalize', help='Normalize text (tokenize, translate, detokenize)', action='store_true')
    parser.add_argument('-T', '--temp', help='Temp directory (default /tmp)', default='/tmp')
    parser.add_argument('-a', '--cache', help='Grammar cache size (default 5)', default='5')
    parser.add_argument('-v', '--verbose', help='Info to stderr', action='store_true')
    parser.add_argument('-D', '--debug-test', help='Test thread safety (debug use only)', action='store_true')
    args = parser.parse_args()

    if args.verbose:
        logging.basicConfig(level=logging.INFO)

    with rt.RealtimeTranslator(args.config, tmpdir=args.temp, cache_size=int(args.cache), norm=args.normalize) as translator:

            # Load state if given
            if args.state:
                with open(args.state) as input:
                    rtd.load_state(input)
            if not args.debug_test:
                run(translator)
            else:
                # TODO: write test
                run(translator)

def run(translator, input=sys.stdin, output=sys.stdout, ctx_name=None):
    # Read lines and commands
    while True:
        line = input.readline()
        if not line:
            break
        line = line.strip()
        if '|||' in line:
            translator.command_line(line, ctx_name)
        else:
            hyp = translator.decode(line, ctx_name)
            output.write('{}\n'.format(hyp))
            output.flush()
 
if __name__ == '__main__':
    main()
