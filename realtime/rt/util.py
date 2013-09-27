import os
import Queue
import subprocess
import sys
import threading

from cdec.configobj import ConfigObj

SA_INI_FILES = set((
    'f_sa_file',
    'e_file',
    'a_file',
    'lex_file',
    'precompute_file',
    ))

class FIFOLock:
    '''Lock that preserves FIFO order of blocking threads'''

    def __init__(self):
        self.q = Queue.Queue()
        self.i = 0
        self.lock = threading.Lock()

    def acquire(self):
        self.lock.acquire()
        self.i += 1
        if self.i > 1:
            event = threading.Event()
            self.q.put(event)
            self.lock.release()
            event.wait()
            return
        self.lock.release()

    def release(self):
        self.lock.acquire()
        self.i -= 1
        if self.i > 0:
            self.q.get().set()
        self.lock.release()

def cdec_ini_for_config(config):
    # This is a list of (k, v), not a ConfigObj or dict
    for i in range(len(config)):
        if config[i][0] == 'feature_function':
            if config[i][1].startswith('KLanguageModel'):
                f = config[i][1].split()
                f[-1] = 'mono.klm'
                config[i][1] = ' '.join(f)
            elif config[i][1].startswith('External'):
                config[i][1] = 'External libcdec_ff_hpyplm.so corpus.hpyplm'

def cdec_ini_for_realtime(config, path, ref_fifo):
    # This is a list of (k, v), not a ConfigObj or dict
    for i in range(len(config)):
        if config[i][0] == 'feature_function':
            if config[i][1].startswith('KLanguageModel'):
                f = config[i][1].split()
                f[-1] = os.path.join(path, f[-1])
                config[i][1] = ' '.join(f)
            elif config[i][1].startswith('External'):
                f = config[i][1].split()
                f[1] =  os.path.join(path, f[1])
                f[2] =  os.path.join(path, f[2])
                f.append('-r')
                f.append(ref_fifo)
                f.append('-t')
                config[i][1] = ' '.join(f)

def consume_stream(stream):
    def consume(s):
        for _ in s:
            pass
    threading.Thread(target=consume, args=(stream,)).start()

def popen_io(cmd):
    p = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    consume_stream(p.stderr)
    return p

def popen_io_v(cmd):
    sys.stderr.write('{}\n'.format(' '.join(cmd)))
    p = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    return p

def sa_ini_for_config(config):
    for key in config:
        if key in SA_INI_FILES:
            config[key] = os.path.join('sa', os.path.basename(config[key]))

def sa_ini_for_realtime(config, path):
    for key in config:
        if key in SA_INI_FILES:
            config[key] = os.path.join(path, config[key])
