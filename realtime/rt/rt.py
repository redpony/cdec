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

import cdec
import aligner
import decoder
import util

class RealtimeDecoder:

    def __init__(self, configdir, tmpdir='/tmp', cache_size=5, norm=False):

        cdec_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

        # Temporary work dir
        self.tmp = tempfile.mkdtemp(dir=tmpdir, prefix='realtime.')
        logging.info('Using temp dir {}'.format(self.tmp))

        # Normalization
        self.norm = norm
        if self.norm:
            self.tokenizer = util.popen_io([os.path.join(cdec_root, 'corpus', 'tokenize-anything.sh'), '-u'])
            self.detokenizer = util.popen_io([os.path.join(cdec_root, 'corpus', 'untok.pl')])

        # Word aligner
        fwd_params = os.path.join(configdir, 'a.fwd_params')
        fwd_err = os.path.join(configdir, 'a.fwd_err')
        rev_params = os.path.join(configdir, 'a.rev_params')
        rev_err = os.path.join(configdir, 'a.rev_err')
        self.aligner = aligner.ForceAligner(fwd_params, fwd_err, rev_params, rev_err)

        # Grammar extractor
        sa_config = cdec.configobj.ConfigObj(os.path.join(configdir, 'sa.ini'), unrepr=True)
        sa_config.filename = os.path.join(self.tmp, 'sa.ini')
        util.sa_ini_for_realtime(sa_config, os.path.abspath(configdir))
        sa_config.write()
        self.extractor = cdec.sa.GrammarExtractor(sa_config.filename, online=True)
        self.grammar_files = collections.deque()
        self.grammar_dict = {}
        self.cache_size = cache_size

        # HPYPLM reference stream
        ref_fifo_file = os.path.join(self.tmp, 'ref.fifo')
        os.mkfifo(ref_fifo_file)
        self.ref_fifo = open(ref_fifo_file, 'w+')
        # Start with empty line (do not learn prior to first input)
        self.ref_fifo.write('\n')
        self.ref_fifo.flush()

        # Decoder
        decoder_config = [[f.strip() for f in line.split('=')] for line in open(os.path.join(configdir, 'cdec.ini'))]
        util.cdec_ini_for_realtime(decoder_config, os.path.abspath(configdir), ref_fifo_file)
        decoder_config_file = os.path.join(self.tmp, 'cdec.ini')
        with open(decoder_config_file, 'w') as output:
            for (k, v) in decoder_config:
                output.write('{}={}\n'.format(k, v))
        decoder_weights = os.path.join(configdir, 'weights.final')
        self.decoder = decoder.MIRADecoder(decoder_config_file, decoder_weights)

    def close(self):
        logging.info('Closing processes')
        self.aligner.close()
        self.decoder.close()
        self.ref_fifo.close()
        if self.norm:
            self.tokenizer.stdin.close()
            self.detokenizer.stdin.close()
        logging.info('Deleting {}'.format(self.tmp))
        shutil.rmtree(self.tmp)

    def grammar(self, sentence):
        grammar_file = self.grammar_dict.get(sentence, None)
        # Cache hit
        if grammar_file:
            logging.info('Grammar cache hit')
            return grammar_file
        # Extract and cache
        grammar_file = tempfile.mkstemp(dir=self.tmp, prefix='grammar.')[1]
        with open(grammar_file, 'w') as output:
            for rule in self.extractor.grammar(sentence):
                output.write('{}\n'.format(str(rule)))
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
        # Empty in, empty out
        if sentence.strip() == '':
            return ''
        if self.norm:
            sentence = self.tokenize(sentence)
            logging.info('Normalized input: {}'.format(sentence))
        grammar_file = self.grammar(sentence)
        start_time = time.time()
        hyp = self.decoder.decode(sentence, grammar_file)
        stop_time = time.time()
        logging.info('Translation time: {} seconds'.format(stop_time - start_time))
        # Empty reference: HPYPLM does not learn prior to next translation
        self.ref_fifo.write('\n')
        self.ref_fifo.flush()
        if self.norm:
            logging.info('Normalized translation: {}'.format(hyp))
            hyp = self.detokenize(hyp)
        return hyp

    def tokenize(self, line):
        self.tokenizer.stdin.write('{}\n'.format(line))
        return self.tokenizer.stdout.readline().strip()

    def detokenize(self, line):
        self.detokenizer.stdin.write('{}\n'.format(line))
        return self.detokenizer.stdout.readline().strip()

    def learn(self, source, target):
        if '' in (source.strip(), target.strip()):
            logging.info('Error empty source or target: {} ||| {}'.format(source, target))
            return
        if self.norm:
            source = self.tokenize(source)
            target = self.tokenize(target)
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
        # Add to HPYPLM by writing to fifo (read on next translation)
        logging.info('Adding to HPYPLM: {}'.format(target))
        self.ref_fifo.write('{}\n'.format(target))
        self.ref_fifo.flush()
