cimport cintlist
from libc.stdio cimport FILE

cdef class DataArray:
	cdef word2id
	cdef id2word
	cdef cintlist.CIntList data
	cdef cintlist.CIntList sent_id
	cdef cintlist.CIntList sent_index
	cdef use_sent_id
	cdef void write_handle(self, FILE* f)
	cdef void read_handle(self, FILE* f)
