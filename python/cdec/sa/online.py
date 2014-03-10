from __future__ import division

import collections
import gzip
import itertools

from cdec.sa._sa import gzip_or_text

# Same as Cython implementation.  Collisions with NULL in bitext?
NULL_WORD = 'NULL'

def learn_vocab(text_f):
    vocab = set()
    for line in gzip_or_text(text_f):
        for word in line.strip().split():
            vocab.add(word)
    return vocab

def write_vocab(vocab, out_f):
    with gzip.open(out_f, 'wb') as out:
        for word in sorted(vocab):
            out.write('{}\n'.format(word))

def read_vocab(in_f):
    return set(line.strip() for line in gzip_or_text(in_f))

class Bilex:

    def __init__(self, in_f=None):
        self.f = collections.defaultdict(int)
        self.e = collections.defaultdict(int)
        self.fe = collections.defaultdict(lambda: collections.defaultdict(int))
        self.ef = collections.defaultdict(lambda: collections.defaultdict(int))
        if in_f:
            self.read(in_f)

    def get_score(self, f, e, dir):
        if dir == 0:
            p = self.p_fe(f, e)
        elif dir == 1:
            p = self.p_ef(e, f)
        return p

    def p_fe(self, f, e):
        d = self.fe.get(f, None)
        if not d:
            return None
        val = d.get(e, None)
        if not val:
            return None
        return val / self.f.get(f)

    def p_ef(self, e, f):
        d = self.ef.get(e, None)
        if not d:
            return None
        val = d.get(f, None)
        if not val:
            return None
        return val / self.e.get(e)

    # Update counts from aligned sentence
    def update(self, f_words, e_words, links):
        covered_f = set()
        covered_e = set()
        for (i, j) in links:
            covered_f.add(i)
            covered_e.add(j)
            self.f[f_words[i]] += 1
            self.e[e_words[j]] += 1
            self.fe[f_words[i]][e_words[j]] += 1
            self.ef[e_words[j]][f_words[i]] += 1
        # e being covered corresponds to f->e
        for j in range(len(e_words)):
            if j not in covered_e:
                self.f[NULL_WORD] += 1
                self.e[e_words[j]] += 1
                self.fe[NULL_WORD][e_words[j]] += 1
                self.ef[e_words[j]][NULL_WORD] += 1
        # e->f
        for i in range(len(f_words)):
            if i not in covered_f:
                self.f[f_words[i]] += 1
                self.e[NULL_WORD] += 1
                self.ef[NULL_WORD][f_words[i]] += 1
                self.fe[f_words[i]][NULL_WORD] += 1

    # Update counts from alignd bitext
    def add_bitext(self, alignment_f, text_f, target_text_f=None):
        # Allow one or two args for bitext
        if target_text_f:
            t = itertools.izip((line.strip() for line in gzip_or_text(text_f)), (line.strip() for line in gzip_or_text(target_text_f)))
        else:
            t = (line.strip().split(' ||| ') for line in gzip_or_text(text_f))
        a = (line.strip() for line in gzip_or_text(alignment_f))
        for (source, target) in t:
            links = sorted(tuple(int(link) for link in link_str.split('-')) for link_str in a.next().split())
            self.update(source.split(), target.split(), links)

    def write(self, out_f):
        fv = sorted(self.f)
        ev = sorted(self.e)
        with gzip.open(out_f, 'wb') as out:
            for f in fv:
                out.write('{} {}\n'.format(f, self.f[f]))
            out.write('\n')
            for e in ev:
                out.write('{} {}\n'.format(e, self.e[e]))
            out.write('\n')
            for f in fv:
                for (e, c) in sorted(self.fe[f].iteritems()):
                    out.write('{} {} {}\n'.format(f, e, c))
            out.write('\n')
            for e in ev:
                for (f, c) in sorted(self.ef[e].iteritems()):
                    out.write('{} {} {}\n'.format(e, f, c))
            out.write('\n')

    def read(self, in_f):
        with gzip_or_text(in_f) as inp:
            while True:
                line = inp.readline().strip()
                if not line:
                    break
                (w, c) = line.split()
                self.f[w] = int(c)
            while True:
                line = inp.readline().strip()
                if not line:
                    break
                (w, c) = line.split()
                self.e[w] = int(c)
            while True:
                line = inp.readline().strip()
                if not line:
                    break
                (f, e, c) = line.split()
                self.fe[f][e] = float(c)
            while True:
                line = inp.readline().strip()
                if not line:
                    break
                (e, f, c) = line.split()
                self.ef[e][f] = float(c)
