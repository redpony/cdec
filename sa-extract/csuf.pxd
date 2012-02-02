cimport cdat
cimport cintlist

cdef class SuffixArray:
	cdef cdat.DataArray darray
	cdef cintlist.CIntList sa
	cdef cintlist.CIntList ha
	cdef __lookup_helper(self, int word_id, int offset, int low, int high)
	cdef __get_range(self, int word_id, int offset, int low, int high, int midpoint)
	cdef __search_low(self, int word_id, int offset, int low, int high)
	cdef __search_high(self, word_id, offset, low, high)
