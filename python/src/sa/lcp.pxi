"""Compute LCP array for a suffix array using the Kasai et al. algorithm
Can also be used to compute statistics such
as k most frequent n-grams"""

cdef class LCP:
    cdef SuffixArray sa
    cdef IntList lcp

    def __cinit__(self, SuffixArray sa):
        cdef int i, k, j, h, n
        cdef IntList rank

        logger.info("Constructing LCP array")
        self.sa = sa
        n = self.sa.sa.len
        self.lcp = IntList(initial_len=n)

        rank = IntList(initial_len=n)
        for i from 0 <= i < n:
            rank.arr[sa.sa.arr[i]] = i

        h = 0
        for i from 0 <= i < n:
            k = rank.arr[i]
            if k == 0:
                self.lcp.arr[k] = -1
            else:
                j = sa.sa.arr[k-1]
                while i+h < n and j+h < n and sa.darray.data.arr[i+h] == sa.darray.data.arr[j+h]:
                    h = h+1
                self.lcp.arr[k] = h
            if h > 0:
                h = h-1
        logger.info("LCP array completed")

    def compute_stats(self, int max_n):
        """Note: the output of this function is not exact.  In
        particular, the frequency associated with each word is 
        not guaranteed to be correct.  This is due to a bit of
        laxness in the design; the function is intended only to
        obtain a list of the most frequent words; for this 
        purpose it is perfectly fine"""
        cdef int i, ii, iii, j, k, h, n, N, rs, freq, valid
        cdef IntList run_start
        cdef IntList ngram_start
        cdef VEB veb
        
        N = self.sa.sa.len

        ngram_starts = []
        for n from 0 <= n < max_n:
            ngram_starts.append(IntList(initial_len=N))

        run_start = IntList(initial_len=max_n)
        veb = VEB(N)

        for i from 0 <= i < N:
            h = self.lcp.arr[i]
            if h < 0:
                h = 0
            for n from h <= n < max_n:
                rs = run_start.arr[n]
                run_start.arr[n] = i
                freq = i - rs
                if freq > 1000: # arbitrary, but see note below
                    veb._insert(freq)
                    ngram_start = ngram_starts[n]
                    while ngram_start.arr[freq] > 0:
                        freq = freq + 1 # cheating a bit, should be ok for sparse histogram
                    ngram_start.arr[freq] = rs
        i = veb.veb.min_val
        while i != -1:
            ii = veb._findsucc(i)
            for n from 0 <= n < max_n:
                ngram_start = ngram_starts[n]
                iii = i
                rs = ngram_start.arr[iii]
                while (ii==-1 or iii < ii) and rs != 0:
                    j = self.sa.sa.arr[rs]
                    valid = 1
                    for k from 0 <= k < n+1:
                        if self.sa.darray.data.arr[j+k] < 2:
                            valid = 0
                    if valid:
                        ngram = tuple([self.sa.darray.data.arr[j+k] for k in range(n+1)])
                        yield i, n+1, ngram
                    iii = iii + 1
                    rs = ngram_start.arr[iii]
            i = ii
