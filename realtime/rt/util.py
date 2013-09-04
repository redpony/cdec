import os
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

def sa_ini_addpath(config, path):
    for key in config:
        if key in SA_INI_FILES:
            config[key] = os.path.join(path, config[key])

def sa_ini_basename(config):
    for key in config:
        if key in SA_INI_FILES:
            config[key] = os.path.join('sa', os.path.basename(config[key]))

def cdec_ini_addpath(config, path):
    cdec_ini_fn(config, lambda x: os.path.join(path, x))

def cdec_ini_basename(config):
    cdec_ini_fn(config, os.path.basename)

def cdec_ini_fn(config, fn):
    # This is a list of (k, v), not a ConfigObj or dict
    for i in range(len(config)):
        if config[i][0] == 'feature_function':
            if config[i][1].startswith('KLanguageModel'):
                f = config[i][1].split()
                f[-1] = fn(f[-1])
                config[i][1] = ' '.join(f)
            elif config[i][1].startswith('External'):
                f = config[i][1].split()
                if f[1].endswith('libcdec_ff_hpyplm.so'):
                    for j in range(1, len(f)):
                        if not f[j].startswith('-'):
                            f[j] = fn(f[j])
                config[i][1] = ' '.join(f)

