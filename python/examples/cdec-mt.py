#!/usr/bin/env python
import sys
import argparse
import logging
import multiprocessing as mp
import cdec

decoder = None
def make_decoder(config, weights):
    global decoder
    decoder = cdec.Decoder(config)
    decoder.read_weights(weights)

def translate(sentence):
    global decoder
    return decoder.translate(sentence).viterbi()

def main():
    logging.basicConfig(level=logging.INFO, format='%(message)s')

    parser = argparse.ArgumentParser(description='Run multiple decoders concurrentely')
    parser.add_argument('-c', '--config', required=True,
                        help='decoder configuration')
    parser.add_argument('-w', '--weights', required=True,
                        help='feature weights')
    parser.add_argument('-j', '--jobs', type=int, default=mp.cpu_count(),
                        help='number of decoder instances')
    parser.add_argument('-s', '--chunksize', type=int, default=10,
                        help='number of sentences / chunk')
    args = parser.parse_args()

    with open(args.config) as config:
        config = config.read()
    logging.info('Starting %d workers; chunk size: %d', args.jobs, args.chunksize)
    pool = mp.Pool(args.jobs, make_decoder, (config, args.weights))
    for output in pool.imap(translate, sys.stdin, args.chunksize):
        print(output.encode('utf8'))
    logging.info('Shutting down workers...')

if __name__ == '__main__':
    main()
