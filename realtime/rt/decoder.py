import logging
import os
import subprocess

import util

class Decoder:

    def close(self):
        self.decoder.stdin.close()

    def decode(self, sentence, grammar):
        input = '<seg grammar="{g}">{s}</seg>\n'.format(s=sentence, g=grammar)
        self.decoder.stdin.write(input)
        return self.decoder.stdout.readline().strip()

class CdecDecoder(Decoder):
    
    def __init__(self, config, weights):
        cdec_root = os.path.dirname(os.path.dirname(os.path.dirname(__file__)))
        decoder = os.path.join(cdec_root, 'decoder', 'cdec')
        decoder_cmd = [decoder, '-c', config, '-w', weights]
        logging.info('Executing: {}'.format(' '.join(decoder_cmd)))
        self.decoder = util.popen_io(decoder_cmd)

class MIRADecoder(Decoder):

    def __init__(self, config, weights):
        cdec_root = os.path.dirname(os.path.dirname(os.path.dirname(__file__)))
        mira = os.path.join(cdec_root, 'training', 'mira', 'kbest_cut_mira')
        #                                              optimizer=2 step=0.001    best=500,    k=500,       uniq, stream
        mira_cmd = [mira, '-c', config, '-w', weights, '-o', '2', '-C', '0.001', '-b', '500', '-k', '500', '-u', '-t']
        logging.info('Executing: {}'.format(' '.join(mira_cmd)))
        self.decoder = util.popen_io(mira_cmd)

    def update(self, sentence, grammar, reference):
        input = '<seg grammar="{g}">{s}</seg> ||| {r}\n'.format(s=sentence, g=grammar, r=reference)
        self.decoder.stdin.write(input)
        return self.decoder.stdout.readline().strip()
