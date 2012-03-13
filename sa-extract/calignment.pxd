cimport cintlist
from libc.stdio cimport FILE

cdef class Alignment:

	cdef cintlist.CIntList links
	cdef cintlist.CIntList sent_index
	cdef int link(self, int i, int j)
	cdef _unlink(self, int link, int* f, int* e)
	cdef int* _get_sent_links(self, int sent_id, int* num_links)
