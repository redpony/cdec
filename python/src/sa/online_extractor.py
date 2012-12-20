#!/usr/bin/env python

import collections, sys

def main(argv):

    for line in sys.stdin:
        src, tgt, astr = (x.split() for x in line.split('|||'))
        al = sorted(tuple(int(y) for y in x.split('-')) for x in astr)
        extract_and_aggr(src, tgt, al)

# Extract hierarchical phrase pairs
# This could be far better optimized by integrating it
# with suffix array code.  For now, it gets the job done.
def extract_and_aggr(src, tgt, al, max_len=5, max_size=15, max_nt=2, boundary_nt=True):
        
    src_ph = collections.defaultdict(lambda: 0) # src = count
    tgt_ph = collections.defaultdict(lambda: 0) # tgt = count
    # [src][tgt] = count
    phrase_pairs = collections.defaultdict(lambda: collections.defaultdict(lambda: 0))
    
    src_w = collections.defaultdict(lambda: 0) # count
    tgt_w = collections.defaultdict(lambda: 0) # count
    # [src][tgt] = count
    cooc_w = collections.defaultdict(lambda: collections.defaultdict(lambda: 0))
    
    # Bilexical counts
    for word in tgt:
        tgt_w[word] += 1
    for word in src:
        src_w[word] += 1
        for t_word in tgt:
            cooc_w[word][t_word] += 1

    def next_nt(nt):
        if not nt:
            return 1
        return nt[-1][0] + 1
    
    src_len = len(src)
    
    a = [[] for i in range(src_len)]
    
    # Pre-compute alignment min and max for each word
    a_span = [[src_len + 1, -1] for i in range(src_len)]
    for (s, t) in al:
        a[s].append(t)
        a_span[s][0] = min(a_span[s][0], t)
        a_span[s][1] = max(a_span[s][1], t)

    # Target side non-terimnal coverage
    # Cython bit vector?
    cover = [0] * src_len
    
    print src
    print tgt
    print a_span
    
    # Spans are _inclusive_ on both ends [i, j]
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

    # Extract all possible hierarchical phrases starting at a source index
    # src i and j are current, tgt i and j are previous
    def extract(src_i, src_j, tgt_i, tgt_j, wc, al, nt, nt_open):
        # Phrase extraction limits
        if wc > max_len or (src_j + 1) >= src_len or \
                (src_j - src_i) + 1 > max_size or len(nt) > max_nt:
            return
        # Unaligned word
        if not a[src_j]:
            # Open non-terminal: extend
            if nt_open:
                nt[-1][2] += 1
                extract(src_i, src_j + 1, tgt_i, tgt_j, wc, al, nt, True)
                nt[-1][2] -= 1
            # No open non-terminal: extend with word
            else:
                extract(src_i, src_j + 1, tgt_i, tgt_j, wc + 1, al, nt, False)
            return
        # Aligned word
        link_i = a_span[src_j][0]
        link_j =  a_span[src_j][1]
        new_tgt_i = min(link_i, tgt_i)
        new_tgt_j = max(link_j, tgt_j)
        # Open non-terminal: close, extract, extend
        if nt_open:
            # Close non-terminal, checking for collisions
            old_last_nt = nt[-1][:]
            nt[-1][2] = src_j
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
            add_rule(src_i, new_tgt_i, src[src_i:src_j + 1], tgt[new_tgt_i:new_tgt_j + 1], nt, al)
            extract(src_i, src_j + 1, new_tgt_i, new_tgt_j, wc, al, nt, False)
            nt[-1] = old_last_nt
            if link_i < nt[-1][3]:
                span_flip(cover, link_i, nt[-1][3] - 1)
            if link_j > nt[-1][4]:
                span_flip(cover, nt[-1][4] + 1, link_j)
            return
        # No open non-terminal
        # Extract, extend with word
        collision = False
        for link in a[src_j]:
            if cover[link]:
                collision = True
        # Collisions block extraction and extension, but may be okay for
        # continuing non-terminals
        if not collision:
            plus_al = []
            for link in a[src_j]:
                plus_al.append((src_j, link))
                cover[link] = ~cover[link]
            al.append(plus_al)
            add_rule(src_i, new_tgt_i, src[src_i:src_j + 1], tgt[new_tgt_i:new_tgt_j + 1], nt, al)
            extract(src_i, src_j + 1, new_tgt_i, new_tgt_j, wc + 1, al, nt, False)
            al.pop()
            for link in a[src_j]:
                cover[link] = ~cover[link]
        # Try to add a word to a (closed) non-terminal, extract, extend
        if nt and nt[-1][2] == src_j - 1:
            # Add to non-terminal, checking for collisions
            old_last_nt = nt[-1][:]
            nt[-1][2] = src_j
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
            if al:
                add_rule(src_i, new_tgt_i, src[src_i:src_j + 1], tgt[new_tgt_i:new_tgt_j + 1], nt, al)
            extract(src_i, src_j + 1, new_tgt_i, new_tgt_j, wc, al, nt, False)
            nt[-1] = old_last_nt
            if new_tgt_i < nt[-1][3]:
                span_flip(cover, link_i, nt[-1][3] - 1)
            if link_j > nt[-1][4]:
                span_flip(cover, nt[-1][4] + 1, link_j)
        # Try to start a new non-terminal, extract, extend
        if not nt or src_j - nt[-1][2] > 1:
            # Check for collisions
            if not span_check(cover, link_i, link_j):
                return
            span_flip(cover, link_i, link_j)
            nt.append([next_nt(nt), src_j, src_j, link_i, link_j])
            # Require at least one word in phrase
            if al:
                add_rule(src_i, new_tgt_i, src[src_i:src_j + 1], tgt[new_tgt_i:new_tgt_j + 1], nt, al)
            extract(src_i, src_j + 1, new_tgt_i, new_tgt_j, wc, al, nt, False)
            nt.pop()
            span_flip(cover, link_i, link_j)
        # TODO: try adding NT to start, end, both
        # check: one aligned word on boundary that is not part of a NT
            
    # Try to extract phrases from every src index
    src_i = 0
    while src_i < src_len:
        # Skip if phrases won't be tight on left side
        if not a[src_i]:
            src_i += 1
            continue
        extract(src_i, src_i, src_len + 1, -1, 1, [], [], False)
        src_i += 1

# Create a rule from source, target, non-terminals, and alignments
def add_rule(src_i, tgt_i, src_span, tgt_span, nt, al):
    flat = (item for sub in al for item in sub)
    astr = ' '.join('{0}-{1}'.format(x[0], x[1]) for x in flat)
    
#    print '--- Rule'
#    print src_span
#    print tgt_span
#    print nt
#    print astr
#    print '---'
    
    # This could be more efficient but is probably not going to
    # be the bottleneck
    src_sym = src_span[:]
    off = src_i
    for next_nt in nt:
        nt_len = (next_nt[2] - next_nt[1]) + 1
        i = 0
        while i < nt_len:
            src_sym.pop(next_nt[1] - off)
            i += 1
        src_sym.insert(next_nt[1] - off, '[X,{0}]'.format(next_nt[0]))
        off += (nt_len - 1)
    tgt_sym = tgt_span[:]
    off = tgt_i
    for next_nt in sorted(nt, cmp=lambda x, y: cmp(x[3], y[3])):
        nt_len = (next_nt[4] - next_nt[3]) + 1
        i = 0
        while i < nt_len:
            tgt_sym.pop(next_nt[3] - off)
            i += 1
        tgt_sym.insert(next_nt[3] - off, '[X,{0}]'.format(next_nt[0]))
        off += (nt_len - 1)
    print '[X] ||| {0} ||| {1} ||| {2}'.format(' '.join(src_sym), ' '.join(tgt_sym), astr)

if __name__ == '__main__':
    main(sys.argv)