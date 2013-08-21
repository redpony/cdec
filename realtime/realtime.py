#!/usr/bin/env python
import argparse
import os
import logging
import cdec.configobj
import cdec.sa
import shutil
import sys
import subprocess
import tempfile
import time

from rt import ForceAligner
from rt import CdecDecoder

class RealtimeDecoder:

    def __init__(self, configdir, tmpdir='/tmp'):

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

        # Decoder
        decoder_config = os.path.join(configdir, 'cdec.ini')
        decoder_weights = os.path.join(configdir, 'weights.final')
        #TODO: run MIRA instead
        self.decoder = CdecDecoder(decoder_config, decoder_weights)

    def close(self):
        logging.info('Closing processes')
        self.aligner.close()
        self.decoder.close()
        logging.info('Deleting {}'.format(self.tmp))
        shutil.rmtree(self.tmp)

    def grammar(self, sentence):
        grammar_file = tempfile.mkstemp(dir=self.tmp)[1]
        with open(grammar_file, 'w') as output:
            for rule in self.extractor.grammar(sentence):
                output.write(str(rule) + '\n')
        return grammar_file
        
    def decode(self, sentence):
        grammar_file = self.grammar(sentence)
        start_time = time.time()
        hyp = self.decoder.decode(sentence, grammar_file)
        stop_time = time.time()
        logging.info('Translation time: {} seconds'.format(stop_time - start_time))
        os.remove(grammar_file)
        return hyp

    def learn(self, source, target):
        alignment = self.aligner.align('{} ||| {}'.format(source, target))
        logging.info('Adding instance: {} ||| {} ||| {}'.format(source, target, alignment))
        self.extractor.add_instance(source, target, alignment)
        # TODO: Add to LM
        # TODO: MIRA update

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
        for line in sys.stdin:
            input = [f.strip() for f in line.split('|||')]
            if len(input) == 1:
                hyp = rtd.decode(input[0])
                sys.stdout.write('{}\n'.format(hyp))
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
