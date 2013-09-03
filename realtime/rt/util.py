import subprocess
import sys
import threading

def popen_io(cmd):
    p = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    consume_stream(p.stderr)
    return p

def popen_io_v(cmd):
    sys.stderr.write('{}\n'.format(' '.join(cmd)))
    p = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    return p

def consume_stream(stream):
    def consume(s):
        for _ in s:
            pass
    threading.Thread(target=consume, args=(stream,)).start()
