#!/usr/bin/env python

import argparse
import logging
import sys

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
    args = parser.parse_args()

    if args.verbose:
        logging.basicConfig(level=logging.INFO)

    with rt.RealtimeDecoder(args.config, tmpdir=args.temp, cache_size=int(args.cache), norm=args.normalize) as rtd:

        try:
            # Load state if given
            if args.state:
                rtd.load_state(args.state)
            # Read lines and commands
            while True:
                line = sys.stdin.readline()
                if not line:
                    break
                line = line.strip()
                if '|||' in line:
                    rtd.command_line(line)
                else:
                    hyp = rtd.decode(line)
                    sys.stdout.write('{}\n'.format(hyp))
                    sys.stdout.flush()
    
        # Clean exit on ctrl+c
        except KeyboardInterrupt:
            logging.info('Caught KeyboardInterrupt, exiting')

if __name__ == '__main__':
    main()
