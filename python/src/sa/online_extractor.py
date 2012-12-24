#!/usr/bin/env python

import collections, sys

import cdec.configobj

CAT = '[X]'  # Default non-terminal
MAX_SIZE = 15  # Max span of a grammar rule (source)
MAX_LEN = 5  # Max number of terminals and non-terminals in a rule (source)
MAX_NT = 2  # Max number of non-terminals in a rule
MIN_GAP = 1  # Min number of terminals between non-terminals (source)

# Spans are _inclusive_ on both ends [i, j]
# TODO: Replace all of this with bit vectors?
def span_check(vec, i, j):
    k = i
    while k <= j:
        if vec[k]:
            return False
        k += 1
    return True

def span_flip(vec, i, j):
    k = i
    while k <= j:
        vec[k] = ~vec[k]
        k += 1

# Next non-terminal
def next_nt(nt):
     if not nt:
         return 1
     return nt[-1][0] + 1

class NonTerminal:
    def __init__(self, index):
        self.index = index
    def __str__(self):
        return '[X,{0}]'.format(self.index)

def fmt_rule(f_sym, e_sym, links):
    a_str = ' '.join('{0}-{1}'.format(i, j) for (i, j) in links)
    return '[X] ||| {0} ||| {1} ||| {2}'.format(' '.join(str(sym) for sym in f_sym),
                                                ' '.join(str(sym) for sym in e_sym),
                                                a_str)

class OnlineGrammarExtractor:

    def __init__(self, config=None):
        if isinstance(config, str) or isinstance(config, unicode):
            if not os.path.exists(config):
                raise IOError('cannot read configuration from {0}'.format(config))
            config = cdec.configobj.ConfigObj(config, unrepr=True)
        elif not config:
            config = collections.defaultdict(lambda: None)
        self.category = CAT
        self.max_size = MAX_SIZE
        self.max_length = config['max_len'] or MAX_LEN
        self.max_nonterminals = config['max_nt'] or MAX_NT
        self.min_gap_size = MIN_GAP
        # Hard coded: require at least one aligned word
        # Hard coded: require tight phrases

        # Phrase counts
        self.phrases_f = collections.defaultdict(lambda: 0)
        self.phrases_e = collections.defaultdict(lambda: 0)
        self.phrases_fe = collections.defaultdict(lambda: collections.defaultdict(lambda: 0))

        # Bilexical counts
        self.bilex_f = collections.defaultdict(lambda: 0)
        self.bilex_e = collections.defaultdict(lambda: 0)
        self.bilex_fe = collections.defaultdict(lambda: collections.defaultdict(lambda: 0))

    # Aggregate bilexical counts
    def aggr_bilex(self, f_words, e_words):

        for e_w in e_words:
            self.bilex_e[e_w] += 1

        for f_w in f_words:
            self.bilex_f[f_w] += 1
            for e_w in e_words:
                self.bilex_fe[f_w][e_w] += 1

    # Aggregate stats from a training instance:
    # Extract hierarchical phrase pairs
    # Update bilexical counts
    def add_instance(self, f_words, e_words, alignment):

        # Bilexical counts
        self.aggr_bilex(f_words, e_words)

        # Phrase pairs extracted from this instance
        phrases = set()

        f_len = len(f_words)
        e_len = len(e_words)

        # Pre-compute alignment info
        al = [[] for i in range(f_len)]
        al_span = [[f_len + 1, -1] for i in range(f_len)]
        for (f, e) in alignment:
            al[f].append(e)
            al_span[f][0] = min(al_span[f][0], e)
            al_span[f][1] = max(al_span[f][1], e)

        # Target side word coverage
        # TODO: Does Cython do bit vectors?
        cover = [0] * e_len

        # Extract all possible hierarchical phrases starting at a source index
        # f_ i and j are current, e_ i and j are previous
        def extract(f_i, f_j, e_i, e_j, wc, links, nt, nt_open):
            # Phrase extraction limits
            if wc + len(nt) > self.max_length or (f_j + 1) > f_len or \
                    (f_j - f_i) + 1 > self.max_size:
                return
            # Unaligned word
            if not al[f_j]:
                # Open non-terminal: extend
                if nt_open:
                    nt[-1][2] += 1
                    extract(f_i, f_j + 1, e_i, e_j, wc, links, nt, True)
                    nt[-1][2] -= 1
                # No open non-terminal: extend with word
                else:
                    extract(f_i, f_j + 1, e_i, e_j, wc + 1, links, nt, False)
                return
            # Aligned word
            link_i = al_span[f_j][0]
            link_j = al_span[f_j][1]
            new_e_i = min(link_i, e_i)
            new_e_j = max(link_j, e_j)
            # Open non-terminal: close, extract, extend
            if nt_open:
                # Close non-terminal, checking for collisions
                old_last_nt = nt[-1][:]
                nt[-1][2] = f_j
                if link_i < nt[-1][3]:
                    if not span_check(cover, link_i, nt[-1][3] - 1):
                        nt[-1] = old_last_nt
                        return
                    span_flip(cover, link_i, nt[-1][3] - 1)
                    nt[-1][3] = link_i
                if link_j > nt[-1][4]:
                    if not span_check(cover, nt[-1][4] + 1, link_j):
                        nt[-1] = old_last_nt
                        return
                    span_flip(cover, nt[-1][4] + 1, link_j)
                    nt[-1][4] = link_j
                for rule in self.form_rules(f_i, new_e_i, f_words[f_i:f_j + 1], e_words[new_e_i:new_e_j + 1], nt, links):
                    phrases.add(rule)
                extract(f_i, f_j + 1, new_e_i, new_e_j, wc, links, nt, False)
                nt[-1] = old_last_nt
                if link_i < nt[-1][3]:
                    span_flip(cover, link_i, nt[-1][3] - 1)
                if link_j > nt[-1][4]:
                    span_flip(cover, nt[-1][4] + 1, link_j)
                return
            # No open non-terminal
            # Extract, extend with word
            collision = False
            for link in al[f_j]:
                if cover[link]:
                    collision = True
            # Collisions block extraction and extension, but may be okay for
            # continuing non-terminals
            if not collision:
                plus_links = []
                for link in al[f_j]:
                    plus_links.append((f_j, link))
                    cover[link] = ~cover[link]
                links.append(plus_links)
                for rule in self.form_rules(f_i, new_e_i, f_words[f_i:f_j + 1], e_words[new_e_i:new_e_j + 1], nt, links):
                    phrases.add(rule)
                extract(f_i, f_j + 1, new_e_i, new_e_j, wc + 1, links, nt, False)
                links.pop()
                for link in al[f_j]:
                    cover[link] = ~cover[link]
            # Try to add a word to a (closed) non-terminal, extract, extend
            if nt and nt[-1][2] == f_j - 1:
                # Add to non-terminal, checking for collisions
                old_last_nt = nt[-1][:]
                nt[-1][2] = f_j
                if link_i < nt[-1][3]:
                    if not span_check(cover, link_i, nt[-1][3] - 1):
                        nt[-1] = old_last_nt
                        return
                    span_flip(cover, link_i, nt[-1][3] - 1)
                    nt[-1][3] = link_i
                if link_j > nt[-1][4]:
                    if not span_check(cover, nt[-1][4] + 1, link_j):
                        nt[-1] = old_last_nt
                        return
                    span_flip(cover, nt[-1][4] + 1, link_j)
                    nt[-1][4] = link_j
                # Require at least one word in phrase
                if links:
                    for rule in self.form_rules(f_i, new_e_i, f_words[f_i:f_j + 1], e_words[new_e_i:new_e_j + 1], nt, links):
                        phrases.add(rule)
                extract(f_i, f_j + 1, new_e_i, new_e_j, wc, links, nt, False)
                nt[-1] = old_last_nt
                if new_e_i < nt[-1][3]:
                    span_flip(cover, link_i, nt[-1][3] - 1)
                if link_j > nt[-1][4]:
                    span_flip(cover, nt[-1][4] + 1, link_j)
            # Try to start a new non-terminal, extract, extend
            if (not nt or f_j - nt[-1][2] > 1) and len(nt) < self.max_nonterminals:
                # Check for collisions
                if not span_check(cover, link_i, link_j):
                    return
                span_flip(cover, link_i, link_j)
                nt.append([next_nt(nt), f_j, f_j, link_i, link_j])
                # Require at least one word in phrase
                if links:
                    for rule in self.form_rules(f_i, new_e_i, f_words[f_i:f_j + 1], e_words[new_e_i:new_e_j + 1], nt, links):
                        phrases.add(rule)
                extract(f_i, f_j + 1, new_e_i, new_e_j, wc, links, nt, False)
                nt.pop()
                span_flip(cover, link_i, link_j)
            # TODO: try adding NT to start, end, both
            # check: one aligned word on boundary that is not part of a NT

        # Try to extract phrases from every f index
        f_i = 0
        while f_i < f_len:
            # Skip if phrases won't be tight on left side
            if not al[f_i]:
                f_i += 1
                continue
            extract(f_i, f_i, f_len + 1, -1, 1, [], [], False)
            f_i += 1

        for rule in sorted(phrases):
            print rule

    # Create a rule from source, target, non-terminals, and alignments
    def form_rules(self, f_i, e_i, f_span, e_span, nt, al):
    
        # This could be more efficient but is unlikely to be the bottleneck
    
        rules = []
    
        nt_inv = sorted(nt, cmp=lambda x, y: cmp(x[3], y[3]))
    
        f_sym = f_span[:]
        off = f_i
        for next_nt in nt:
            nt_len = (next_nt[2] - next_nt[1]) + 1
            i = 0
            while i < nt_len:
                f_sym.pop(next_nt[1] - off)
                i += 1
            f_sym.insert(next_nt[1] - off, NonTerminal(next_nt[0]))
            off += (nt_len - 1)
    
        e_sym = e_span[:]
        off = e_i
        for next_nt in nt_inv:
            nt_len = (next_nt[4] - next_nt[3]) + 1
            i = 0
            while i < nt_len:
                e_sym.pop(next_nt[3] - off)
                i += 1
            e_sym.insert(next_nt[3] - off, NonTerminal(next_nt[0]))
            off += (nt_len - 1)
    
        # Adjusting alignment links takes some doing
        links = [list(link) for sub in al for link in sub]
        links_len = len(links)
        nt_len = len(nt)
        nt_i = 0
        off = f_i
        i = 0
        while i < links_len:
            while nt_i < nt_len and links[i][0] > nt[nt_i][1]:
                off += (nt[nt_i][2] - nt[nt_i][1])
                nt_i += 1
            links[i][0] -= off
            i += 1
        nt_i = 0
        off = e_i
        i = 0
        while i < links_len:
            while nt_i < nt_len and links[i][1] > nt_inv[nt_i][3]:
                off += (nt_inv[nt_i][4] - nt_inv[nt_i][3])
                nt_i += 1
            links[i][1] -= off
            i += 1
    
        # Rule
        rules.append(fmt_rule(f_sym, e_sym, links))
        if len(f_sym) >= self.max_length or len(nt) >= self.max_nonterminals:
            return rules
        last_index = nt[-1][0] if nt else 0
        # Rule [X]
        if not nt or not isinstance(f_sym[-1], NonTerminal):
            f_sym.append(NonTerminal(last_index + 1))
            e_sym.append(NonTerminal(last_index + 1))
            rules.append(fmt_rule(f_sym, e_sym, links))
            f_sym.pop()
            e_sym.pop()
        # [X] Rule
        if not nt or not isinstance(f_sym[0], NonTerminal):
            for sym in f_sym:
                if isinstance(sym, NonTerminal):
                    sym.index += 1
            for sym in e_sym:
                if isinstance(sym, NonTerminal):
                    sym.index += 1
            for link in links:
                link[0] += 1
                link[1] += 1
            f_sym.insert(0, NonTerminal(1))
            e_sym.insert(0, NonTerminal(1))
            rules.append(fmt_rule(f_sym, e_sym, links))
        if len(f_sym) >= self.max_length or len(nt) + 1 >= self.max_nonterminals:
            return rules
        # [X] Rule [X]
        if not nt or not isinstance(f_sym[-1], NonTerminal):
            f_sym.append(NonTerminal(last_index + 2))
            e_sym.append(NonTerminal(last_index + 2))
            rules.append(fmt_rule(f_sym, e_sym, links))
        return rules

def main(argv):

    extractor = OnlineGrammarExtractor()

    for line in sys.stdin:
        print >> sys.stderr, line.strip()
        f_words, e_words, a_str = (x.split() for x in line.split('|||'))
        alignment = sorted(tuple(int(y) for y in x.split('-')) for x in a_str)
        extractor.add_instance(f_words, e_words, alignment)

if __name__ == '__main__':
    main(sys.argv)
