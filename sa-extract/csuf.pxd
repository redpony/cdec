cimport cdat
cimport cintlist

cdef class SuffixArray:
	cdef cdat.DataArray darray
	cdef cintlist.CIntList sa
	cdef cintlist.CIntList ha
	# cdef lookup(self, word, int offset, int low, int high)
	cdef __lookup_helper(self, int word_id, int offset, int low, int high)
	cdef __get_range(self, int word_id, int offset, int low, int high, int midpoint)
	cdef int __search_low(self, int word_id, int offset, int low, int high)
	cdef int __search_high(self, int word_id, int offset, int low, int high)
