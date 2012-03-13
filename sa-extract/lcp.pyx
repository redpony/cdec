#!/usr/bin/env python2.4

'''Compute LCP array for a suffix array using the Kasai et al. algorithm'''
'''Can also be used to compute statistics such
as k most frequent n-grams'''

import sys

cimport cintlist
cimport csuf
cimport cdat
cimport cveb

cdef class LCP:

	cdef csuf.SuffixArray sa
	cdef cintlist.CIntList lcp

	def __init__(self, sa):
		self._construct(sa)

	cdef _construct(self, csuf.SuffixArray sa):
		cdef int i, k, j, h, n
		cdef cintlist.CIntList rank

		sys.stderr.write("Constructing LCP array\n")
		self.sa = sa
		n = self.sa.sa.len
		self.lcp = cintlist.CIntList(initial_len=n)

		rank = cintlist.CIntList(initial_len=n)
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
		sys.stderr.write("LCP array completed\n")


	def compute_stats(self, max_n):
		self._compute_stats(max_n)

	cdef _compute_stats(self, int max_n):
		'''Note: the output of this function is not exact.  In
		particular, the frequency associated with each word is 
		not guaranteed to be correct.  This is due to a bit of
		laxness in the design; the function is intended only to
		obtain a list of the most frequent words; for this 
		purpose it is perfectly fine'''
		cdef int i, ii, iii, j, k, h, n, N, rs, freq, valid
		cdef cintlist.CIntList run_start
		cdef cintlist.CIntList ngram_start
		cdef cveb.VEB veb
		
		N = self.sa.sa.len

		ngram_starts = []
		for n from 0 <= n < max_n:
			ngram_starts.append(cintlist.CIntList(initial_len=N))

		run_start = cintlist.CIntList(initial_len=max_n)
		veb = cveb.VEB(N)

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
						ngram = ""
						for k from 0 <= k < n+1:
							ngram= ngram+ self.sa.darray.id2word[self.sa.darray.data.arr[j+k]] + " "
						print i, n+1, ngram
					iii = iii + 1
					rs = ngram_start.arr[iii]
			i = ii





