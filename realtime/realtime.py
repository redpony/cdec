#!/usr/bin/env python

import argparse
import collections
import logging
import os
import shutil
import sys
import subprocess
import tempfile
import time

import cdec.configobj
import cdec.sa

from rt import ForceAligner
from rt import MIRADecoder

class RealtimeDecoder:

    def __init__(self, configdir, tmpdir='/tmp', cache_size=5):

        # Temporary work dir
        self.tmp = tempfile.mkdtemp(dir=tmpdir)
        logging.info('Using temp dir {}'.format(self.tmp))

        # Word aligner
        fwd_params = os.path.join(configdir, 'a.fwd_params')
        fwd_err = os.path.join(configdir, 'a.fwd_err')
        rev_params = os.path.join(configdir, 'a.rev_params')
        rev_err = os.path.join(configdir, 'a.rev_err')
        self.aligner = ForceAligner(fwd_params, fwd_err, rev_params, rev_err)

        # Grammar extractor
        sa_config = os.path.join(configdir, 'sa.ini')
        self.extractor = cdec.sa.GrammarExtractor(sa_config, online=True)
        self.grammar_files = collections.deque()
        self.grammar_dict = {}
        self.cache_size = cache_size

        # Decoder
        decoder_config = os.path.join(configdir, 'cdec.ini')
        decoder_weights = os.path.join(configdir, 'weights.final')
        self.decoder = MIRADecoder(decoder_config, decoder_weights)

    def close(self):
        logging.info('Closing processes')
        self.aligner.close()
        self.decoder.close()
        logging.info('Deleting {}'.format(self.tmp))
        shutil.rmtree(self.tmp)

    def grammar(self, sentence):
        grammar_file = self.grammar_dict.get(sentence, None)
        # Cache hit
        if grammar_file:
            logging.info('Grammar cache hit')
            return grammar_file
        # Extract and cache
        grammar_file = tempfile.mkstemp(dir=self.tmp)[1]
        with open(grammar_file, 'w') as output:
            for rule in self.extractor.grammar(sentence):
                output.write(str(rule) + '\n')
        if len(self.grammar_files) == self.cache_size:
            rm_sent = self.grammar_files.popleft()
            # If not already removed by learn method
            if rm_sent in self.grammar_dict:
                rm_grammar = self.grammar_dict.pop(rm_sent)
                os.remove(rm_grammar)
        self.grammar_files.append(sentence)
        self.grammar_dict[sentence] = grammar_file
        return grammar_file
        
    def decode(self, sentence):
        grammar_file = self.grammar(sentence)
        start_time = time.time()
        hyp = self.decoder.decode(sentence, grammar_file)
        stop_time = time.time()
        logging.info('Translation time: {} seconds'.format(stop_time - start_time))
        return hyp

    def learn(self, source, target):
        # MIRA update before adding data to grammar extractor
        grammar_file = self.grammar(source)
        mira_log = self.decoder.update(source, grammar_file, target)
        logging.info('MIRA: {}'.format(mira_log))
        # Add aligned sentence pair to grammar extractor
        alignment = self.aligner.align(source, target)
        logging.info('Adding instance: {} ||| {} ||| {}'.format(source, target, alignment))
        self.extractor.add_instance(source, target, alignment)
        # Clear (old) cached grammar
        rm_grammar = self.grammar_dict.pop(source)
        os.remove(rm_grammar)
        # TODO: Add to LM by writing to fifo

def main():

    parser = argparse.ArgumentParser(description='Real-time adaptive translation with cdec.')
    parser.add_argument('-c', '--config', required=True, help='Config directory (see README.md)')
    parser.add_argument('-T', '--temp', help='Temp directory (default /tmp)', default='/tmp')
    parser.add_argument('-v', '--verbose', help='Info to stderr', action='store_true')
    args = parser.parse_args()

    if not args.config:
        parser.error('specify a configuration directory')

    if args.verbose:
        logging.basicConfig(level=logging.INFO)

    rtd = RealtimeDecoder(args.config)

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


def mkconfig():
    pass

if __name__ == '__main__':
    main()
