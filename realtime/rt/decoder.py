import logging
import os
import subprocess
import threading

import util

logger = logging.getLogger('rt.decoder')

class Decoder:

    def close(self, force=False):
        if not force:
            self.lock.acquire()
        self.decoder.stdin.close()
        self.decoder.wait()
        if not force:
            self.lock.release()

    def decode(self, sentence, grammar=None):
        '''Threadsafe, FIFO'''
        self.lock.acquire()
        input = '<seg grammar="{g}">{s}</seg>\n'.format(s=sentence, g=grammar) if grammar else '{}\n'.format(sentence)
        self.decoder.stdin.write(input)
        hyp = self.decoder.stdout.readline().strip()
        self.lock.release()
        return hyp

class CdecDecoder(Decoder):

    def __init__(self, config, weights):
        cdec_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
        decoder = os.path.join(cdec_root, 'decoder', 'cdec')
        decoder_cmd = [decoder, '-c', config, '-w', weights]
        logger.info('Executing: {}'.format(' '.join(decoder_cmd)))
        self.decoder = util.popen_io(decoder_cmd)
        self.lock = util.FIFOLock()

class MIRADecoder(Decoder):

    def __init__(self, config, weights, metric='ibm_bleu'):
        cdec_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
        mira = os.path.join(cdec_root, 'training', 'mira', 'kbest_cut_mira')
        #                                              optimizer=2 step=0.001    best=500,    k=500,       uniq, stream, metric
        mira_cmd = [mira, '-c', config, '-w', weights, '-o', '2', '-C', '0.001', '-b', '500', '-k', '500', '-u', '-t', '-m', metric]
        logger.info('Executing: {}'.format(' '.join(mira_cmd)))
        self.decoder = util.popen_io(mira_cmd)
        self.lock = util.FIFOLock()

    def get_weights(self):
        '''Threadsafe, FIFO'''
        self.lock.acquire()
        self.decoder.stdin.write('WEIGHTS ||| WRITE\n')
        weights = self.decoder.stdout.readline().strip()
        self.lock.release()
        return weights

    def set_weights(self, w_line):
        '''Threadsafe, FIFO'''
        self.lock.acquire()
        try:
            # Check validity
            for w_str in w_line.split():
                (k, v) = w_str.split('=')
                float(v)
            self.decoder.stdin.write('WEIGHTS ||| {}\n'.format(w_line))
            self.lock.release()
        except:
            self.lock.release()
            raise Exception('Invalid weights line: {}'.format(w_line))


    def update(self, sentence, grammar, reference):
        '''Threadsafe, FIFO'''
        self.lock.acquire()
        input = 'LEARN ||| <seg grammar="{g}">{s}</seg> ||| {r}\n'.format(s=sentence, g=grammar, r=reference)
        self.decoder.stdin.write(input)
        log = self.decoder.stdout.readline().strip()
        self.lock.release()
        return log
