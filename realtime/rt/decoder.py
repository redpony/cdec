import os
import subprocess

import util

class Decoder:

    def close(self):
        self.decoder.stdin.close()

class CdecDecoder(Decoder):
    
    def __init__(self, config, weights):
        cdec_root = os.path.dirname(os.path.dirname(os.path.dirname(__file__)))
        decoder = os.path.join(cdec_root, 'decoder', 'cdec')
        decoder_cmd = [decoder, '-c', config, '-w', weights]
        self.decoder = util.popen_io(decoder_cmd)

    def decode(self, sentence, grammar):
        input = '<seg grammar="{g}">{s}</seg>\n'.format(i=id, s=sentence, g=grammar)
        self.decoder.stdin.write(input)
        return self.decoder.stdout.readline().strip()

