#!/usr/bin/env python

import argparse
import logging
import sys

import rt

def main():

    parser = argparse.ArgumentParser(description='Real-time adaptive translation with cdec.')
    parser.add_argument('-c', '--config', required=True, help='Config directory (see README.md)')
    parser.add_argument('-T', '--temp', help='Temp directory (default /tmp)', default='/tmp')
    parser.add_argument('-a', '--cache', help='Grammar cache size (default 5)', default='5')
    parser.add_argument('-v', '--verbose', help='Info to stderr', action='store_true')
    args = parser.parse_args()

    if not args.config:
        parser.error('specify a configuration directory')

    if args.verbose:
        logging.basicConfig(level=logging.INFO)

    rtd = rt.RealtimeDecoder(args.config, tmpdir=args.temp, cache_size=int(args.cache))

    try:
        while True:
            line = sys.stdin.readline()
            if not line:
                break
            input = [f.strip() for f in line.split('|||')]
            if len(input) == 1:
                hyp = rtd.decode(input[0])
                sys.stdout.write('{}\n'.format(hyp))
                sys.stdout.flush()
            elif len(input) == 2:
                rtd.learn(*input)

    # Clean exit on ctrl+c
    except KeyboardInterrupt:
        logging.info('Caught KeyboardInterrupt, exiting')

    # Cleanup
    rtd.close()

if __name__ == '__main__':
    main()
