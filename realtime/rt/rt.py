#!/usr/bin/env python

import argparse
import collections
import logging
import os
import shutil
import sys
import subprocess
import tempfile
import threading
import time

import cdec
import aligner
import decoder
import util

# Dummy input token that is unlikely to appear in normalized data (but no fatal errors if it does)
LIKELY_OOV = '(OOV)'

class RealtimeDecoder:
    '''Do not use directly unless you know what you're doing.  Use RealtimeTranslator.'''

    def __init__(self, configdir, tmpdir):

        cdec_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

        self.tmp = tmpdir
        os.mkdir(self.tmp)

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

    def close(self, force=False):
        logging.info('Closing decoder and removing {}'.format(self.tmp))
        self.decoder.close(force)
        self.ref_fifo.close()
        shutil.rmtree(self.tmp)

class RealtimeTranslator:
    '''Main entry point into API: serves translations to any number of concurrent users'''

    def __init__(self, configdir, tmpdir='/tmp', cache_size=5, norm=False, state=None):

        # TODO: save/load
        self.commands = {'LEARN': self.learn, 'SAVE': self.save_state, 'LOAD': self.load_state}

        cdec_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

        ### Single instance for all contexts

        self.config = configdir
        # Temporary work dir
        self.tmp = tempfile.mkdtemp(dir=tmpdir, prefix='realtime.')
        logging.info('Using temp dir {}'.format(self.tmp))

        # Normalization
        self.norm = norm
        if self.norm:
            self.tokenizer = util.popen_io([os.path.join(cdec_root, 'corpus', 'tokenize-anything.sh'), '-u'])
            self.tokenizer_sem = threading.Semaphore()
            self.detokenizer = util.popen_io([os.path.join(cdec_root, 'corpus', 'untok.pl')])
            self.detokenizer_sem = threading.Semaphore()

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
        self.cache_size = cache_size

        ### One instance per context

        self.ctx_names = set()
        # All context-dependent operations are atomic
        self.ctx_sems = collections.defaultdict(threading.Semaphore)
        # ctx -> list of (source, target, alignment)
        self.ctx_data = {}

        # ctx -> deque of file
        self.grammar_files = {}
        # ctx -> dict of {sentence: file}
        self.grammar_dict = {}

        self.decoders = {}

        # TODO: state
        # Load state if given
        if state:
            with open(state) as input:
                self.load_state(input)

    def __enter__(self):
        return self

    def __exit__(self, ex_type, ex_value, ex_traceback):
        self.close(ex_type is KeyboardInterrupt)

    def close(self, force=False):
        '''Cleanup'''
        if force:
            logging.info('Forced shutdown: stopping immediately')
        for ctx_name in list(self.ctx_names):
            self.drop_ctx(ctx_name, force)
        logging.info('Closing processes')
        self.aligner.close()
        if self.norm:
            self.tokenizer.stdin.close()
            self.detokenizer.stdin.close()
        logging.info('Deleting {}'.format(self.tmp))
        shutil.rmtree(self.tmp)

    def lazy_ctx(self, ctx_name):
        '''Initialize a context (inc starting a new decoder) if needed'''
        self.ctx_sems[ctx_name].acquire()
        if ctx_name in self.ctx_names:
            self.ctx_sems[ctx_name].release()
            return
        logging.info('New context: {}'.format(ctx_name))
        self.ctx_names.add(ctx_name)
        self.ctx_data[ctx_name] = []
        self.grammar_files[ctx_name] = collections.deque()
        self.grammar_dict[ctx_name] = {}
        tmpdir = os.path.join(self.tmp, 'decoder.{}'.format(ctx_name))
        self.decoders[ctx_name] = RealtimeDecoder(self.config, tmpdir)
        self.ctx_sems[ctx_name].release()

    def drop_ctx(self, ctx_name, force=False):
        '''Delete a context (inc stopping the decoder)'''
        if not force:
            sem = self.ctx_sems[ctx_name]
            sem.acquire()
        logging.info('Dropping context: {}'.format(ctx_name))
        self.ctx_names.remove(ctx_name)
        self.ctx_data.pop(ctx_name)
        self.extractor.drop_ctx(ctx_name)
        self.grammar_files.pop(ctx_name)
        self.grammar_dict.pop(ctx_name)
        self.decoders.pop(ctx_name).close(force)
        self.ctx_sems.pop(ctx_name)
        if not force:
            sem.release()
        
    def grammar(self, sentence, ctx_name=None):
        '''Extract a sentence-level grammar on demand (or return cached)'''
        self.lazy_ctx(ctx_name)
        sem = self.ctx_sems[ctx_name]
        sem.acquire()
        grammar_dict = self.grammar_dict[ctx_name]
        grammar_file = grammar_dict.get(sentence, None)
        # Cache hit
        if grammar_file:
            logging.info('Grammar cache hit: {}'.format(grammar_file))
            sem.release()
            return grammar_file
        # Extract and cache
        (fid, grammar_file) = tempfile.mkstemp(dir=self.decoders[ctx_name].tmp, prefix='grammar.')
        os.close(fid)
        with open(grammar_file, 'w') as output:
            for rule in self.extractor.grammar(sentence, ctx_name):
                output.write('{}\n'.format(str(rule)))
        grammar_files = self.grammar_files[ctx_name]
        if len(grammar_files) == self.cache_size:
            rm_sent = grammar_files.popleft()
            # If not already removed by learn method
            if rm_sent in grammar_dict:
                rm_grammar = grammar_dict.pop(rm_sent)
                os.remove(rm_grammar)
        grammar_files.append(sentence)
        grammar_dict[sentence] = grammar_file
        sem.release()
        return grammar_file
        
    def decode(self, sentence, ctx_name=None):
        '''Decode a sentence (inc extracting a grammar if needed)'''
        self.lazy_ctx(ctx_name)
        # Empty in, empty out
        if sentence.strip() == '':
            return ''
        if self.norm:
            sentence = self.tokenize(sentence)
            logging.info('Normalized input: {}'.format(sentence))
        # grammar method is threadsafe 
        grammar_file = self.grammar(sentence, ctx_name)
        decoder = self.decoders[ctx_name]
        sem = self.ctx_sems[ctx_name]
        sem.acquire()
        start_time = time.time()
        hyp = decoder.decoder.decode(sentence, grammar_file)
        stop_time = time.time()
        logging.info('Translation time: {} seconds'.format(stop_time - start_time))
        # Empty reference: HPYPLM does not learn prior to next translation
        decoder.ref_fifo.write('\n')
        decoder.ref_fifo.flush()
        sem.release()
        if self.norm:
            logging.info('Normalized translation: {}'.format(hyp))
            hyp = self.detokenize(hyp)
        return hyp

    def tokenize(self, line):
        self.tokenizer_sem.acquire()
        self.tokenizer.stdin.write('{}\n'.format(line))
        tok_line = self.tokenizer.stdout.readline().strip()
        self.tokenizer_sem.release()
        return tok_line

    def detokenize(self, line):
        self.detokenizer_sem.acquire()
        self.detokenizer.stdin.write('{}\n'.format(line))
        detok_line = self.detokenizer.stdout.readline().strip()
        self.detokenizer_sem.release()
        return detok_line

    # TODO
    def command_line(self, line, ctx_name=None):
        args = [f.strip() for f in line.split('|||')]
        try:
            if len(args) == 2 and not args[1]:
                self.commands[args[0]](ctx_name)
            else:
                self.commands[args[0]](*args[1:], ctx_name=ctx_name)
        except:
            logging.info('Command error: {}'.format(' ||| '.join(args)))
        
    def learn(self, source, target, ctx_name=None):
        self.lazy_ctx(ctx_name)
        if '' in (source.strip(), target.strip()):
            logging.info('Error empty source or target: {} ||| {}'.format(source, target))
            return
        if self.norm:
            source = self.tokenize(source)
            target = self.tokenize(target)
        # Align instance (threadsafe)
        alignment = self.aligner.align(source, target)
        # grammar method is threadsafe
        grammar_file = self.grammar(source, ctx_name)
        sem = self.ctx_sems[ctx_name]
        sem.acquire()
        # MIRA update before adding data to grammar extractor
        decoder = self.decoders[ctx_name]
        mira_log = decoder.decoder.update(source, grammar_file, target)
        logging.info('MIRA: {}'.format(mira_log))
        # Add to HPYPLM by writing to fifo (read on next translation)
        logging.info('Adding to HPYPLM: {}'.format(target))
        decoder.ref_fifo.write('{}\n'.format(target))
        decoder.ref_fifo.flush()
        # Store incremental data for save/load
        self.ctx_data[ctx_name].append((source, target, alignment))
        # Add aligned sentence pair to grammar extractor
        logging.info('Adding to bitext: {} ||| {} ||| {}'.format(source, target, alignment))
        self.extractor.add_instance(source, target, alignment, ctx_name)
        # Clear (old) cached grammar
        rm_grammar = self.grammar_dict[ctx_name].pop(source)
        os.remove(rm_grammar)
        sem.release()

    def save_state(self, filename=None, ctx_name=None):
        self.lazy_ctx(ctx_name)
        out = open(filename, 'w') if filename else sys.stdout
        sem = self.ctx_sems[ctx_name]
        sem.acquire()
        ctx_data = self.ctx_data[ctx_name]
        logging.info('Saving state with {} sentences'.format(len(self.ctx_data)))
        out.write('{}\n'.format(self.decoders[ctx_name].decoder.get_weights()))
        for (source, target, alignment) in ctx_data:
            out.write('{} ||| {} ||| {}\n'.format(source, target, alignment))
        sem.release()
        out.write('EOF\n')
        if filename:
            out.close()

    def load_state(self, input=sys.stdin, ctx_name=None):
        self.lazy_ctx(ctx_name)
        sem = self.ctx_sems[ctx_name]
        sem.acquire() 
        ctx_data = self.ctx_data[ctx_name]
        decoder = self.decoders[ctx_name]
       # Non-initial load error
        if ctx_data:
            logging.info('Error: Incremental data has already been added to decoder.')
            logging.info('       State can only be loaded by a freshly started decoder.')
            return
        # MIRA weights
        line = input.readline().strip()
        decoder.decoder.set_weights(line)
        logging.info('Loading state...')
        start_time = time.time()
        # Lines source ||| target ||| alignment
        while True:
            line = input.readline().strip()
            if line == 'EOF':
                break
            (source, target, alignment) = line.split(' ||| ')
            ctx_data.append((source, target, alignment))
            # Extractor
            self.extractor.add_instance(source, target, alignment, ctx_name)
            # HPYPLM
            hyp = decoder.decoder.decode(LIKELY_OOV)
            self.ref_fifo.write('{}\n'.format(target))
            self.ref_fifo.flush()
        stop_time = time.time()
        logging.info('Loaded state with {} sentences in {} seconds'.format(len(ctx_data), stop_time - start_time))
        sem.release()
