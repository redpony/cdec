#!/usr/bin/env python

import os
import re
import subprocess
import sys

# Tokenize XML files with tokenize-anything.sh
# in:  <seg id="963"> The earnings on its 10-year bonds are 28.45%. </seg>
# out: <seg id="963"> The earnings on its 10 - year bonds are 28.45 % . </seg>

def escape(s):
    return s.replace('&', '&amp;').replace('>', '&gt;').replace('<', '&lt;').replace('"', '&quot;').replace('\'', '&apos;')

def unescape(s):
    return s.replace('&gt;', '>').replace('&lt;', '<').replace('&quot;', '"').replace('&apos;', '\'').replace('&amp;', '&')

def main():
    tok = subprocess.Popen([os.path.join(os.path.dirname(__file__), 'tokenize-anything.sh'), '-u'], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    while True:
        line = sys.stdin.readline()
        if not line:
            break
        line = line.strip()
        pieces = []
        eol = len(line)
        pos = 0
        while pos < eol:
            next = line.find('<', pos)
            if next == -1:
                next = eol
            tok.stdin.write('{}\n'.format(unescape(line[pos:next])))
            pieces.append(escape(tok.stdout.readline().strip()))
            if next == eol:
                break
            pos = line.find('>', next + 1)
            if pos == -1:
                pos = eol
            else:
                pos += 1
            pieces.append(line[next:pos])
        sys.stdout.write('{}\n'.format(' '.join(pieces).strip()))
    tok.stdin.close()
    tok.wait()

if __name__ == '__main__':
    main()
